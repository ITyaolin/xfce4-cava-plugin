/*  $Id$
 *
 *  Copyright (C) 2019 John Doo <john@foo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include <string.h>
#include <math.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include "plugin.h"
#include "dialogs.h"
#include "cava/util.h"

/* the website url */
#define PLUGIN_WEBSITE "https://github.com/kreddkrikk/xfce4-cava-plugin"

static void plugin_configure_response(
        GtkWidget *dialog, gint response, CavaPlugin *c) {
    gboolean result;

    if (response == GTK_RESPONSE_HELP)
    {
        /* show help */
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
        result = g_spawn_command_line_async(
                "xfce-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);
#else
        result = g_spawn_command_line_async(
                "exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);
#endif
        if (G_UNLIKELY (result == FALSE))
            g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
    else
    {
        /* remove the dialog data from the plugin */
        g_object_set_data (G_OBJECT (c->plugin), "dialog", NULL);

        /* save the plugin */
        plugin_save(c);
        profile_save(c);

        /* destroy the properties dialog */
        gtk_widget_destroy (dialog);
    }
}

enum {
    UPDATE_NONE = 0, // no need for manual update
    UPDATE_STYLES = 1, // update CSS styles
    UPDATE_SIZE = 2, // resize display
    UPDATE_COLORS = 4, // reconfigure bar colors
    UPDATE_CONFIG = 8, // reallocate buffers and cava plan
    UPDATE_ALL = 16, // reconfigure and reallocate everything
} UpdateEvent;

typedef struct {
    CavaPlugin *cava;
    gpointer setting;
    gint update_event;
} SettingChanged;

static void set_color_button(GtkWidget *widget, gchar *setting) {
    GdkRGBA color;
    rgba_parse(&color, setting);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(widget), &color);
}

static void load_settings(CavaPlugin *c) {
    CavaSettings *s = &c->settings;
    SettingWidgets *w = &c->widgets;

    /* general */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->framerate), s->framerate);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->sensitivity), s->sensitivity);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->bars), s->bars); 
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->bar_width), s->bar_width);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->bar_spacing), s->bar_spacing);
    gtk_combo_box_set_active(GTK_COMBO_BOX(w->bar_shape), s->bar_shape);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->lower_cutoff_freq), 
            s->lower_cutoff_freq);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->higher_cutoff_freq), 
            s->higher_cutoff_freq);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->sleep_timer), s->sleep_timer);

    /* input */

    /* output */
    gtk_combo_box_set_active(GTK_COMBO_BOX(w->orientation), s->orientation);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->stereo), s->stereo);

    /* color */
    set_color_button(w->background, s->background);
    set_color_button(w->foreground, s->foreground);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->gradient), s->gradient);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->horizontal_gradient), 
            s->horizontal_gradient);
    for (int i = 0; i < GRADIENT_COLOR_COUNT; i++) {
        set_color_button(w->gradient_colors[i], s->gradient_colors[i]);
        set_color_button(w->horizontal_gradient_colors[i], 
                s->horizontal_gradient_colors[i]);
    }

    /* smoothing */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->monstercat), s->monstercat);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->waves), s->waves);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->noise_reduction), 
            s->noise_reduction);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->equalizer), s->equalizer);
    for (int i = 0; i < EQUALIZER_KEY_COUNT; i++) {
        gtk_range_set_value(GTK_RANGE(w->equalizer_keys[i]), 
                s->equalizer_keys[i]);
    }
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->border), s->border);
    set_color_button(w->border_color, s->border_color);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->margin), s->margin);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->padding), s->padding);

    /* update all */
    resize_display(c);
    free_cava(c);
    config_cava(c);
}

static void setting_changed(SettingChanged *sc) {
    gint u = sc->update_event;
    if (u == UPDATE_NONE)
        return;
    if (u & UPDATE_SIZE)
        resize_display(sc->cava);
    if (u & UPDATE_STYLES)
        restyle_display(sc->cava);
    if (u & UPDATE_COLORS)
        config_colors(sc->cava);
    if (u & UPDATE_CONFIG) {
        free_cava(sc->cava);
        config_cava(sc->cava); // includes colors update
    }
    if (u & UPDATE_ALL) {
        resize_display(sc->cava);
        free_cava(sc->cava);
        config_cava(sc->cava);
    }
}

#define SETTING_CHANGED_INIT(widget, event, cb) \
    SettingChanged *sc = g_slice_new0(SettingChanged); \
    sc->cava = c; \
    sc->setting = setting; \
    sc->update_event = update_event; \
    g_signal_connect(widget, event, G_CALLBACK(cb), sc);

static gchar* rgba_to_html(GdkRGBA *c) {
    return g_strdup_printf("#%02x%02x%02x%02x", 
            (int)(c->red * 255),
            (int)(c->green * 255),
            (int)(c->blue * 255),
            (int)(c->alpha * 255));
}

void rgba_parse(GdkRGBA *c, gchar *spec) {
    guint hex;
    size_t len = 0;
    if (spec[0] == '#') {
        len = strnlen(spec, 9);
        if (len == 7) {
            gdk_rgba_parse(c, spec);
        }
        else if (len == 9) {
            /* GDK 3 cannot parse alpha channels in HTML hexadecimal codes 
             * so we must do it ourselves. */
            hex = (guint)g_ascii_strtoll(&spec[1], NULL, 16);
            c->red = (double)(hex >> 24) / 255.0;
            c->green = (double)((hex >> 16) & 0xff) / 255.0;
            c->blue = (double)((hex >> 8) & 0xff) / 255.0;
            c->alpha = (double)(hex & 0xff) / 255.0;
        }
    }
    else {
        gdk_rgba_parse(c, spec);
    }
}

static gboolean validate_spin_button(gint value, SettingChanged *sc) {
    CavaSettings *s = &sc->cava->settings;
    if (sc->setting == &s->lower_cutoff_freq)
        return value < s->higher_cutoff_freq;
    else if (sc->setting == &s->higher_cutoff_freq)
        return value > s->lower_cutoff_freq;
    return TRUE;
}

static void spin_button_changed(GtkSpinButton *self, SettingChanged *sc) {
    gint value = gtk_spin_button_get_value_as_int(self);
    if (value != *(gint *)sc->setting) {
        if (validate_spin_button(value, sc)) {
            *(gint *)sc->setting = value;
            setting_changed(sc);
        }
        else {
           gtk_spin_button_set_value(self, *(gint *)sc->setting); 
        }
    }
}

static void combo_box_changed(GtkComboBox *self, SettingChanged *sc) {
    gint value = gtk_combo_box_get_active(self);
    if (value != *(gint *)sc->setting) {
        *(gint *)sc->setting = value;
        setting_changed(sc);
    }
}

static void check_button_changed(GtkToggleButton *self, SettingChanged *sc) {
    gint value = gtk_toggle_button_get_active(self);
    if (value != *(gint *)sc->setting) {
        *(gint *)sc->setting = value;
        setting_changed(sc);
    }
}

static void color_button_changed(GtkColorButton *self, SettingChanged *sc) {
    GdkRGBA old_color;
    GdkRGBA new_color;
    rgba_parse(&old_color, (gchar *)sc->setting);
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(self), &new_color);
    if (!gdk_rgba_equal(&old_color, &new_color)) {
        *(gchar **)sc->setting = rgba_to_html(&new_color);
        setting_changed(sc);
    }
}

static void scale_value_changed(GtkScale *self, SettingChanged *sc) {
    gdouble value = gtk_range_get_value(GTK_RANGE(self));
    if (value != *(gint *)sc->setting) {
        *(gdouble *)sc->setting = value;
        setting_changed(sc);
    }
}

static void reset_equalizer_button(GtkButton *self, CavaPlugin *c) {
    reset_equalizer(c);
    for (int i = 0; i < EQUALIZER_KEY_COUNT; i++) {
        gtk_range_set_value(GTK_RANGE(c->widgets.equalizer_keys[i]), 
                c->settings.equalizer_keys[i]);
    }
}

static void profile_combo_box_changed(GtkComboBoxText *self, CavaPlugin *c) {
    profile_save(c);
    c->settings.profile = gtk_combo_box_text_get_active_text(self);
    profile_read(c);
    load_settings(c);
}

static gboolean entry_key_press(GtkWidget *self, 
        GdkEventKey *event, CavaPlugin *c) {
    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(self));
    if (event->keyval == GDK_KEY_Return) {
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    }
    return FALSE;
}

static gint show_message_box(GtkWidget *self, gchar *title, gchar *message, 
        GtkButtonsType buttons) {
    /* create dialog */
    GtkWidget *parent_window = gtk_widget_get_toplevel(self);
    GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(parent_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            buttons, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return result;
}

static gchar *show_dialog_with_entry(GtkWidget *self, CavaPlugin *c, 
        gchar *title, gchar *label_text, gchar *entry_text) {
    /* create dialog */
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), entry_text);
    GtkWidget *parent_window = gtk_widget_get_toplevel(GTK_WIDGET(self));
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
            title,
            GTK_WINDOW(parent_window), 
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "OK", GTK_RESPONSE_OK,
            "Cancel", GTK_RESPONSE_CANCEL,
            NULL);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);
    g_signal_connect(entry, "key-press-event", G_CALLBACK(entry_key_press), c);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(label_text);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 8);
    gtk_box_pack_end(GTK_BOX(content_area), hbox, FALSE, FALSE, 8);
    gtk_widget_show_all(dialog);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gchar *text = NULL;
    if (result == GTK_RESPONSE_OK) {
        text = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }
    gtk_widget_destroy(dialog);
    return text;
}

static gboolean profile_exists(gchar *profile_name) {
    gchar *profile_path = get_profile_path(profile_name);
    return profile_path != NULL && g_file_test(
            profile_path, G_FILE_TEST_EXISTS);
}

static void add_new_profile(GtkWidget *self, CavaPlugin *c, 
        gchar *profile_name) {
    gint result;
    SettingWidgets *w = &c->widgets;
    if (profile_name[0] == '\0')
        return;
    if (profile_exists(profile_name)) {
        result = show_message_box(GTK_WIDGET(self), "Replace Profile?", 
                "A profile with this name already exists. Replace?", 
                GTK_BUTTONS_YES_NO);
        if (result != GTK_RESPONSE_YES)
            return;
    }
    c->profile_count++;
    c->settings.profile = profile_name;
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w->profile), 
            profile_name, profile_name);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(w->profile), profile_name);
    gtk_widget_set_sensitive(GTK_WIDGET(w->del_profile), TRUE);
    profile_save(c);
}

static void rename_profile(GtkWidget *self, CavaPlugin *c,
        gchar *profile_name) {
    gint result;
    SettingWidgets *w = &c->widgets;
    if (profile_name[0] == '\0')
        return;
    if (g_strcmp0(c->settings.profile, profile_name) == 0)
        return;
    if (profile_exists(profile_name)) {
        result = show_message_box(GTK_WIDGET(self), "Replace Profile?", 
                "A profile with this name already exists. Replace?", 
                GTK_BUTTONS_YES_NO);
        if (result != GTK_RESPONSE_YES)
            return;
    }
    gchar *new_profile_path = get_profile_path(profile_name);
    if (new_profile_path == NULL)
        return;
    gchar *old_profile_path = get_profile_path(c->settings.profile);
    if (old_profile_path == NULL)
        return;

    /* update profiles */
    c->settings.profile = profile_name;
    gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(w->profile));
    gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(w->profile), active);
    gtk_combo_box_text_insert(GTK_COMBO_BOX_TEXT(w->profile), 
            active, profile_name, profile_name);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(w->profile), profile_name);
    rename(old_profile_path, new_profile_path);
    profile_save(c);
}

static void new_profile_button_clicked(GtkButton *self, CavaPlugin *c) {
    /* create dialog */
    gchar *result = show_dialog_with_entry(GTK_WIDGET(self), c, 
            "New Profile", "Profile name:", "");

    /* update profiles */
    if (result != NULL) {
        add_new_profile(GTK_WIDGET(self), c, result);
        g_free(result);
    }
}

static void del_profile_button_clicked(GtkButton *self, CavaPlugin *c) {
    /* create dialog */
    gint result = show_message_box(GTK_WIDGET(self), "Delete Profile",
            "Are you sure you want to delete the selected profile?\n"
            "This action cannot be undone.", GTK_BUTTONS_YES_NO);

    /* update profiles */
    gchar *profile_path;
    SettingWidgets *w = &c->widgets;
    if (result == GTK_RESPONSE_YES) {
        profile_path = get_profile_path(c->settings.profile);
        if (profile_path != NULL) {
            c->profile_count--;
            gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(w->profile), 
                    gtk_combo_box_get_active(GTK_COMBO_BOX(w->profile)));
            gtk_combo_box_set_active(GTK_COMBO_BOX(w->profile), 0);
            gtk_widget_set_sensitive(GTK_WIDGET(self), c->profile_count > 1);
            remove(profile_path);
        }
    }
}

static void ren_profile_button_clicked(GtkButton *self, CavaPlugin *c) {
    /* create dialog */
    gchar *result = show_dialog_with_entry(GTK_WIDGET(self), c, 
            "Rename Profile", "Profile name:", c->settings.profile);

    /* update profiles */
    if (result != NULL) {
        rename_profile(GTK_WIDGET(self), c, result);
        g_free(result);
    }
}

// Prepends a widget with a label, packs it into a single row or column and 
// appends the packed row or column to a parent container box.
//
// container: parent container box
// line: horizontal box if row, vertical box if column
// widget: the widget to label
// sg: size group to add label for equally-sized columns
// text: text of the label
// xalign: x-alignment of label text
// expand: expand the widget to fill all available space
static void label_and_pack_widget(GtkWidget *container, GtkWidget *line, 
        GtkWidget *widget, GtkSizeGroup *sg, gchar *text, gdouble xalign, 
        gboolean expand) {
    if (text != NULL) {
        GtkWidget *label = gtk_label_new(text);
        gtk_box_pack_start(GTK_BOX(line), label, FALSE, FALSE, 0);
        gtk_label_set_xalign(GTK_LABEL(label), xalign);
        gtk_label_set_yalign(GTK_LABEL(label), 0.5);
        if (sg != NULL)
            gtk_size_group_add_widget(sg, GTK_WIDGET(label));
    }
    gtk_box_pack_start(GTK_BOX(container), GTK_WIDGET(line), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(line), widget, expand, expand, 0);
}

static GtkWidget *create_spin_button(CavaPlugin *c, GtkWidget *container, 
        GtkSizeGroup *sg, gint update_event, gchar *text, gint *setting, 
        gint min, gint max) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *button = gtk_spin_button_new_with_range(min, max, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(button), *setting);
    label_and_pack_widget(container, row, button, sg, text, 0.0, FALSE);
    SETTING_CHANGED_INIT(button, "value-changed", spin_button_changed);
    return button;
}

static GtkWidget *create_check_button(CavaPlugin *c, GtkWidget *container, 
        GtkSizeGroup *sg, gint update_event, gchar *text, gint *setting) {
    GtkWidget *button = gtk_check_button_new_with_mnemonic(text);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *setting);
    gtk_box_pack_start(GTK_BOX(container), GTK_WIDGET(button), FALSE, FALSE, 0);
    SETTING_CHANGED_INIT(button, "toggled", check_button_changed);
    return button;
}

static GtkWidget *create_combo_box(CavaPlugin *c, GtkWidget *container, 
        GtkSizeGroup *sg, gint update_event, gchar *text, 
        const gchar *items[], gint count, gint *setting) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *combo = gtk_combo_box_text_new();
    for (int i = 0; i < count; i++)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), NULL, items[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), *setting);
    label_and_pack_widget(container, row, combo, sg, text, 0.0, FALSE);
    SETTING_CHANGED_INIT(combo, "changed", combo_box_changed);
    return combo;
}

static GtkWidget *create_color_button(CavaPlugin *c, GtkWidget *container, 
        GtkSizeGroup *sg, gint update_event, gchar *text, gchar **setting) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GdkRGBA color;
    rgba_parse(&color, *setting);
    GtkWidget *button = gtk_color_button_new_with_rgba(&color);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(button), TRUE);
    label_and_pack_widget(container, row, button, sg, text, 0.0, FALSE);
    SETTING_CHANGED_INIT(button, "color-set", color_button_changed);
    return button;
}

static GtkWidget *create_scale(CavaPlugin *c, GtkWidget *container, 
        GtkSizeGroup *sg, gint update_event, gchar *text, gdouble *setting, 
        gdouble min, gdouble max, gdouble step) {
    GtkWidget *column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkAdjustment *adjustment = gtk_adjustment_new(
            *setting, min, max, step, 0, 0);
    GtkWidget *scale = gtk_scale_new(GTK_ORIENTATION_VERTICAL, adjustment);
    gtk_range_set_round_digits(GTK_RANGE(scale), 1);
    gtk_range_set_inverted(GTK_RANGE(scale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_BOTTOM);
    label_and_pack_widget(container, column, scale, sg, text, 0.5, TRUE);
    SETTING_CHANGED_INIT(scale, "value-changed", scale_value_changed);
    return scale;
}

static void create_reset_button(CavaPlugin *c, GtkWidget *row, gchar *text, 
        gpointer cb) {
    GtkWidget *button = gtk_button_new_with_label(text);
    gtk_box_pack_start(GTK_BOX(row), GTK_WIDGET(button), FALSE, FALSE, 0);
    g_signal_connect(button, "clicked", G_CALLBACK(cb), c);
}

static void populate_profile_combo_box(CavaPlugin *c) {
    GDir *dir;
    GError *error;
    gchar *profile_name;
    gint profile_index = 0;
    const gchar *filename;

    /* initialize directory */
    CavaSettings *s = &c->settings;
    gchar *profile_dir = get_profile_dir();
    if (profile_dir == NULL)
        return;
    dir = g_dir_open(profile_dir, 0, &error);
    GtkWidget *widget = c->widgets.profile;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widget));

    /* build profile list */
    gint counter = 0;
    for (; (filename = g_dir_read_name(dir)); counter++) {
        if (g_str_has_suffix(filename, ".rc")) {
            profile_name = g_strsplit(filename, ".rc", 0)[0];
            gtk_combo_box_text_append(
                    GTK_COMBO_BOX_TEXT(widget), profile_name, profile_name);
            if (g_str_equal(profile_name, s->profile)) {
                profile_index = counter;
            }
        }
    }
    if (counter == 0) {
        // no profiles found, create new profile
        gtk_combo_box_text_append(
                GTK_COMBO_BOX_TEXT(widget), s->profile, s->profile);
        counter++;
    }
    c->profile_count = counter;
     
    /* update widgets */
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), profile_index);
    g_signal_connect(widget, "changed", G_CALLBACK(profile_combo_box_changed), c);
    g_free(profile_dir);
}

static double logspace(double start, double stop, int n, int N) {
    return start * pow(stop / start, n / (double)(N - 1));
}

void plugin_configure (XfcePanelPlugin *plugin, CavaPlugin *c) {
    GtkWidget *dialog;

    CavaSettings *s = &c->settings;
    SettingWidgets *w = &c->widgets;

    if (c->settings_dialog != NULL)
    {
        gtk_window_present (GTK_WINDOW (c->settings_dialog));
        return;
    }

    // Main window
    c->settings_dialog = dialog = xfce_titled_dialog_new_with_mixed_buttons(
            _("Cava Plugin"),
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            "window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
            NULL);
    g_object_add_weak_pointer(G_OBJECT(c->settings_dialog), 
            (gpointer *)&c->settings_dialog);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), "xfce4-settings");
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    g_object_set_data (G_OBJECT (plugin), "dialog", dialog);
    g_signal_connect (G_OBJECT (dialog), "response",
            G_CALLBACK(plugin_configure_response), c);

    // Bars
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    w->bars = create_spin_button(
            c, vbox, sg, UPDATE_ALL, "Bars:", &s->bars, 1, 512);
    w->bar_width = create_spin_button(
            c, vbox, sg, UPDATE_SIZE, "Bar Width:", &s->bar_width, 1, 100);
    w->bar_spacing = create_spin_button(
            c, vbox, sg, UPDATE_SIZE, "Bar Spacing:", &s->bar_spacing, 0, 10);
    const gchar *bar_shape_items[] = {
        "rectangle",
        "oblong",
    };
    w->bar_shape = create_combo_box(c, vbox, sg, UPDATE_NONE, "Bar Shape:", 
            bar_shape_items, ARRAY_SIZE(bar_shape_items), &s->bar_shape);

    // Orientation
    const gchar *orientation_items[] = {
        "bottom",
        "top",
        "left",
        "right",
        "horizontal",
        "vertical",
    };
    w->orientation = create_combo_box(c, vbox, sg, 
            UPDATE_SIZE | UPDATE_COLORS, "Orientation:", 
            orientation_items, ARRAY_SIZE(orientation_items), &s->orientation);
    gtk_box_pack_start(GTK_BOX(vbox), 
            gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);

    // CSS
    GtkWidget *container;
    w->border = create_spin_button(
            c, vbox, sg, UPDATE_STYLES | UPDATE_SIZE, 
            "Border:", &s->border, 0, 10);
    container = gtk_widget_get_parent(w->border);
    w->border_color = create_color_button(
            c, container, NULL, UPDATE_STYLES, NULL, &s->border_color);
    w->margin = create_spin_button(
            c, vbox, sg, UPDATE_STYLES | UPDATE_SIZE, "Margin:", &s->margin, 0, 100);
    w->padding = create_spin_button(
            c, vbox, sg, UPDATE_STYLES | UPDATE_SIZE, "Padding:", &s->padding, 0, 100);
    gtk_box_pack_start(GTK_BOX(vbox), 
            gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    w->background = create_color_button(
            c, hbox, sg, UPDATE_STYLES, "Background:", &s->background);
    w->foreground = create_color_button(
            c, hbox, NULL, UPDATE_COLORS, "Foreground:", &s->foreground);
    gtk_widget_set_hexpand(w->foreground, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    // Colors
    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    GtkWidget *vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 8);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *vbox2a = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    // Gradient Colors
    w->gradient = create_check_button(
            c, vbox2a, sg, UPDATE_COLORS, "Gradient", &s->gradient);
    gchar *text;
    for (int i = 0; i < GRADIENT_COLOR_COUNT; i++) {
        text = g_strdup_printf("Color %d:", i + 1);
        w->gradient_colors[i] = create_color_button(c, vbox2a, sg, 
                UPDATE_COLORS, text, &s->gradient_colors[i]);
        g_free(text);
    }

    // Horizontal Gradient Colors
    GtkWidget *vbox2b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    w->horizontal_gradient = create_check_button(c, vbox2b, sg, UPDATE_COLORS, 
            "Horizontal Gradient", &s->horizontal_gradient);
    for (int i = 0; i < GRADIENT_COLOR_COUNT; i++) {
        text = g_strdup_printf("Color %d:", i + 1);
        w->horizontal_gradient_colors[i] = create_color_button(c, vbox2b, sg, 
                UPDATE_COLORS, text, &s->horizontal_gradient_colors[i]);
        g_free(text);
    }
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox2a), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), 
            gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox2b), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(hbox), FALSE, FALSE, 0);

    // Equalizer
    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox3), 8);
    w->equalizer = create_check_button(
            c, hbox, NULL, UPDATE_NONE, "Enable", &s->equalizer);
    create_reset_button(c, hbox, "Reset", reset_equalizer_button);
    gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(hbox), FALSE, FALSE, 0);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gdouble freq;
    for (int i = 0; i < EQUALIZER_KEY_COUNT; i++) {
        freq = logspace(s->lower_cutoff_freq, s->higher_cutoff_freq, i, 
                EQUALIZER_KEY_COUNT);
        text = g_strdup_printf("%d", (int)freq);
        w->equalizer_keys[i] = create_scale(c, hbox, sg, UPDATE_NONE, NULL, 
                &s->equalizer_keys[i], 0.0, EQUALIZER_MAX, 0.1);
        g_free(text);
    }
    gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(hbox), TRUE, TRUE, 0);

    // Advanced
    GtkWidget *vbox4 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox4), 8);
    w->framerate = create_spin_button(
            c, vbox4, sg, UPDATE_CONFIG, "Frame Rate:", &s->framerate, 1, 1000);
    w->sleep_timer = create_spin_button(
            c, vbox4, sg, UPDATE_NONE, "Sleep Timer (s):", &s->sleep_timer, 0, 1000);
    w->sensitivity = create_spin_button(
            c, vbox4, sg, UPDATE_NONE, "Sensitivity (%):", &s->sensitivity, 1, 1000);
    w->lower_cutoff_freq = create_spin_button(
            c, vbox4, sg, UPDATE_CONFIG, "Low frequency (Hz):", &s->lower_cutoff_freq, 0, 22000);
    w->higher_cutoff_freq = create_spin_button(
            c, vbox4, sg, UPDATE_CONFIG, "High frequency (Hz):", &s->higher_cutoff_freq, 0, 22000);
    gtk_box_pack_start(GTK_BOX(vbox4), 
            gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);
    w->stereo = create_check_button(
            c, vbox4, sg, UPDATE_ALL, "Stereo", &s->stereo);
    w->monstercat = create_spin_button(
            c, vbox4, sg, UPDATE_NONE, "Smoothing (%):", &s->monstercat, 0, 100);
    w->waves = create_check_button(
            c, vbox4, sg, UPDATE_NONE, "Waves", &s->waves);
    w->noise_reduction = create_spin_button(
            c, vbox4, sg, UPDATE_ALL, "Noise Reduction (%):", &s->noise_reduction, 0, 100);

    // Tab Pages
    GtkWidget* notebook = gtk_notebook_new();

    gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
            GTK_WIDGET(vbox), gtk_label_new("Bars"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
            GTK_WIDGET(vbox2), gtk_label_new("Gradients"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
            GTK_WIDGET(vbox3), gtk_label_new("Equalizer"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
            GTK_WIDGET(vbox4), gtk_label_new("Advanced"));
    gtk_widget_set_vexpand(vbox3, TRUE);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(main), 8);

    // Profile
    GtkWidget *icon;
    w->profile = gtk_combo_box_text_new();
    populate_profile_combo_box(c);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    label_and_pack_widget(main, hbox, w->profile, NULL, "Profile:", 0.0, FALSE);

    w->ren_profile = gtk_button_new();
    icon = gtk_image_new_from_icon_name("document-edit", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(w->ren_profile), icon);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(w->ren_profile), FALSE, FALSE, 0);
    g_signal_connect(
            w->ren_profile, "clicked", G_CALLBACK(ren_profile_button_clicked), c);
    gtk_widget_set_tooltip_text(w->ren_profile, "Rename profile");

    w->del_profile = gtk_button_new();
    icon = gtk_image_new_from_icon_name("edit-delete", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(w->del_profile), icon);
    gtk_widget_set_sensitive(GTK_WIDGET(w->del_profile), c->profile_count > 1);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(w->del_profile), FALSE, FALSE, 0);
    g_signal_connect(
            w->del_profile, "clicked", G_CALLBACK(del_profile_button_clicked), c);
    gtk_widget_set_tooltip_text(w->del_profile, "Delete profile");

    w->new_profile = gtk_button_new();
    icon = gtk_image_new_from_icon_name("document-new", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(w->new_profile), icon);
    gtk_box_pack_start(
            GTK_BOX(hbox), GTK_WIDGET(w->new_profile), FALSE, FALSE, 0);
    g_signal_connect(
            w->new_profile, "clicked", G_CALLBACK(new_profile_button_clicked), c);
    gtk_widget_set_tooltip_text(w->new_profile, "New profile");

    // Done
    gtk_box_pack_start(GTK_BOX(main), notebook, FALSE, FALSE, 8);
    gtk_container_add(GTK_CONTAINER(content), main);

    gtk_widget_show_all(main);

    /* show the entire dialog */
    gtk_widget_show (dialog);
}

void plugin_about (XfcePanelPlugin *plugin) {
    /* about dialog code. you can use the GtkAboutDialog
     * or the XfceAboutInfo widget */
    const gchar *auth[] =
    {
        "Xfce development team <xfce4-dev@xfce.org>",
        NULL
    };

    gtk_show_about_dialog (NULL,
            "logo-icon-name", "xfce4-cava-plugin",
            "license",        xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
            "version",        VERSION_FULL,
            "program-name",   PACKAGE_NAME,
            "comments",       _("CAVA plugin for xfce4-panel"),
            "website",        PLUGIN_WEBSITE,
            "copyright",      "Copyright \xc2\xa9 2006-" COPYRIGHT_YEAR " The Xfce development team",
            "authors",        auth,
            NULL);
}

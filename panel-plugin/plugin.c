/*  $Id$
 *
 *  Copyright(C) 2019 John Doo <john@foo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <ctype.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include "plugin.h"
#include "dialogs.h"

/* default settings */
const gint default_framerate = 60;
const gint default_autosens = 1;
const gint default_overshoot = 20;
const gint default_sensitivity = 100;
const gint default_bars = 32;
const gint default_bar_width = 2;
const gint default_bar_spacing = 1;
const gint default_bar_shape = BAR_SHAPE_RECTANGLE;
const gint default_bar_caps = 0;
const gint default_max_height = 100;
const gint default_lower_cutoff_freq = 60;
const gint default_higher_cutoff_freq = 16000;
const gint default_sleep_timer = 1;
const gint default_input = INPUT_PIPEWIRE; //INPUT_PULSE;
gchar *default_source = "auto";
const gint default_sample_rate = 44100;
const gint default_sample_bits = 16;
const gint default_channels = 2;
const gint default_autoconnect = 2;
const gint default_active = 0;
const gint default_remix = 1;
const gint default_virtual = 1;
const gint default_orientation = ORIENT_SPLIT_H;
const gint default_size = 100;
const gint default_stereo = 0;
const gint default_mono_option = AVERAGE;
const gint default_reverse = 0;
const gint default_show_idle_bar_heads = 0;
const gint default_waveform = 0;
gchar *default_background = "#00000000";
gchar *default_cap_color = "#00ffff";
gchar *default_foreground1 = "#3fffff";
gchar *default_foreground2 = "#1f8f8f";
const gint default_foreground = FG_STYLE_ONE_COLOR;
gchar *default_vgradient_colors[] = {
    "#59cc33",
    "#80cc33",
    "#a6cc33",
    "#cccc33",
    "#cca633",
    "#cc8033",
    "#cc5933",
    "#cc3333",
    NULL
};
gchar *default_hgradient_colors[] = {
    "#c45161",
    "#e094a0",
    "#f2b6c0",
    "#f2dde1",
    "#cbc7d8",
    "#8db7d2",
    "#5e62a9",
    "#434279",
    NULL
};
const gint default_blend_direction = ORIENT_BOTTOM;
gchar *default_theme = "none";
const gint default_integral = 77;
const gint default_monstercat = 0;
const gint default_waves = 0;
const gint default_noise_reduction = 77;
const gint default_equalizer = 0;
const gdouble default_equalizer_key = 1.0;
const gint default_border = 0;
gchar *default_border_color = "#3fffffff";
const gint default_margin = 10;
const gint default_padding = 0;
gchar *default_profile = "cava";

/* prototypes */
static void plugin_construct(XfcePanelPlugin *plugin);

/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER(plugin_construct);

gchar *get_profile_dir(void) {
    gchar *dir = g_strdup_printf(
            "%s/.config/cava/profiles", g_get_home_dir());
    if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
        if (g_mkdir_with_parents(dir, 0755) == -1) {
            printf("Failed to create profile directory.\n");
            g_free(dir);
            return NULL;
        }
    }
    return dir;
}

gchar *get_profile_path(gchar *profile) {
    if (profile == NULL)
        return NULL;
    gchar *dir = get_profile_dir();
    if (dir == NULL)
        return NULL;
    return g_strdup_printf("%s/%s.rc", dir, profile);
}

void plugin_save(CavaPlugin *c) {
    XfceRc *rc;
    gchar *file;
    CavaSettings *s;

    s = &c->settings;

    file = xfce_panel_plugin_save_location(c->plugin, TRUE);

    if (G_UNLIKELY(file == NULL))
    {
        DBG("Failed to open config file");
        return;
    }

    rc = xfce_rc_simple_open(file, FALSE);
    g_free(file);

    if (G_LIKELY(rc != NULL))
    {
        DBG(".");
        xfce_rc_write_entry(rc, "profile", s->profile);
        xfce_rc_close(rc);
    }
}

void profile_save(CavaPlugin *c) {
    XfceRc *rc;
    gchar *file;
    CavaSettings *s;

    s = &c->settings;

    file = get_profile_path(s->profile);

    if (G_UNLIKELY(file == NULL))
    {
        DBG("Failed to open profile");
        return;
    }

    if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
        if (creat(file, 0644) == -1) {
            DBG("Failed to create profile.");
            g_free(file);
            return;
        }
    }

    /* open the config file, read/write */
    rc = xfce_rc_simple_open(file, FALSE);
    g_free(file);

    if (G_LIKELY(rc != NULL))
    {
        /* save the settings */
        DBG(".");

        xfce_rc_write_int_entry(rc, "framerate", s->framerate);
        xfce_rc_write_int_entry(rc, "autosens", s->autosens);
        xfce_rc_write_int_entry(rc, "overshoot", s->overshoot);
        xfce_rc_write_int_entry(rc, "sensitivity", s->sensitivity);
        xfce_rc_write_int_entry(rc, "bars", s->bars);
        xfce_rc_write_int_entry(rc, "bar_width", s->bar_width);
        xfce_rc_write_int_entry(rc, "bar_spacing", s->bar_spacing);
        xfce_rc_write_int_entry(rc, "bar_shape", s->bar_shape);
        xfce_rc_write_int_entry(rc, "bar_caps", s->bar_caps);
        xfce_rc_write_int_entry(rc, "max_height", s->max_height);
        xfce_rc_write_int_entry(rc, "lower_cutoff_freq", s->lower_cutoff_freq);
        xfce_rc_write_int_entry(rc, "higher_cutoff_freq", s->higher_cutoff_freq);
        xfce_rc_write_int_entry(rc, "sleep_timer", s->sleep_timer);
        xfce_rc_write_int_entry(rc, "input", s->input);
        xfce_rc_write_entry(rc, "source", s->source);
        xfce_rc_write_int_entry(rc, "sample_rate", s->sample_rate);
        xfce_rc_write_int_entry(rc, "sample_bits", s->sample_bits);
        xfce_rc_write_int_entry(rc, "channels", s->channels);
        xfce_rc_write_int_entry(rc, "autoconnect", s->autoconnect);
        xfce_rc_write_int_entry(rc, "active", s->active);
        xfce_rc_write_int_entry(rc, "remix", s->remix);
        xfce_rc_write_int_entry(rc, "virtual", s->virtual);
        xfce_rc_write_int_entry(rc, "orientation", s->orientation);
        xfce_rc_write_int_entry(rc, "size", s->size);
        xfce_rc_write_int_entry(rc, "stereo", s->stereo);
        xfce_rc_write_int_entry(rc, "mono_option", s->mono_option);
        xfce_rc_write_int_entry(rc, "reverse", s->reverse);
        xfce_rc_write_int_entry(rc, "show_idle_bar_heads", s->show_idle_bar_heads);
        xfce_rc_write_int_entry(rc, "waveform", s->waveform);
        xfce_rc_write_entry(rc, "background", s->background);
        xfce_rc_write_entry(rc, "cap_color", s->cap_color);
        xfce_rc_write_entry(rc, "foreground1", s->foreground1);
        xfce_rc_write_entry(rc, "foreground2", s->foreground2);
        xfce_rc_write_int_entry(rc, "foreground", s->foreground);
        gchar **colors = g_new0(gchar*, GRADIENT_COLOR_COUNT + 1);
        for (int i = 0; i < GRADIENT_COLOR_COUNT; i++)
            colors[i] = g_strdup(s->vgradient_colors[i]);
        xfce_rc_write_list_entry(rc, "vgradient_colors", colors, ",");
        g_strfreev(colors);
        colors = g_new0(gchar*, GRADIENT_COLOR_COUNT + 1);
        for (int i = 0; i < GRADIENT_COLOR_COUNT; i++)
            colors[i] = g_strdup(s->hgradient_colors[i]);
        xfce_rc_write_list_entry(rc, "hgradient_colors", colors, ",");
        g_strfreev(colors);
        xfce_rc_write_int_entry(rc, "blend_direction", s->blend_direction);
        xfce_rc_write_entry(rc, "theme", s->theme);
        xfce_rc_write_int_entry(rc, "monstercat", s->monstercat);
        xfce_rc_write_int_entry(rc, "waves", s->waves);
        xfce_rc_write_int_entry(rc, "noise_reduction", s->noise_reduction);
        xfce_rc_write_int_entry(rc, "equalizer", s->equalizer);
        gchar **keys = g_new0(gchar*, EQUALIZER_KEY_COUNT + 1);
        for (int i = 0; i < EQUALIZER_KEY_COUNT; i++)
            keys[i] = g_strdup_printf("%d", (gint)(s->equalizer_keys[i] * 10.0));
        xfce_rc_write_list_entry(rc, "equalizer_keys", keys, ",");
        g_strfreev(keys);
        xfce_rc_write_int_entry(rc, "border", s->border);
        xfce_rc_write_entry(rc, "border_color", s->border_color);
        xfce_rc_write_int_entry(rc, "margin", s->margin);
        xfce_rc_write_int_entry(rc, "padding", s->padding);

        /* close the rc file */
        xfce_rc_close(rc);
    }
}

void plugin_read(CavaPlugin *c) {
    XfceRc *rc;
    gchar *file;
    CavaSettings *s;

    s = &c->settings;

    DBG(".");

    /* get the plugin config file location */
    file = xfce_panel_plugin_save_location(c->plugin, TRUE);
    if (G_LIKELY(file != NULL))
    {
        /* open the config file, readonly */
        rc = xfce_rc_simple_open(file, TRUE);

        /* cleanup */
        g_free(file);

        if (G_LIKELY(rc != NULL))
        {
            s->profile = g_strdup(xfce_rc_read_entry(rc, "profile", default_profile));
            xfce_rc_close(rc);
            return;
        }
    }

    DBG("Applying default settings");

    s->profile = g_strdup(default_profile);
}

void profile_read(CavaPlugin *c) {
    XfceRc *rc;
    gchar *file;
    CavaSettings *s;

    s = &c->settings;

    DBG(".");

    file = get_profile_path(s->profile);

    if (G_LIKELY(file != NULL))
    {
        /* open the config file, readonly */
        rc = xfce_rc_simple_open(file, TRUE);

        /* cleanup */
        g_free(file);

        if (G_LIKELY(rc != NULL))
        {
            /* read the settings */
            s->framerate = xfce_rc_read_int_entry(rc, "framerate", default_framerate);
            s->autosens = xfce_rc_read_int_entry(rc, "autosens", default_autosens);
            s->overshoot = xfce_rc_read_int_entry(rc, "overshoot", default_overshoot);
            s->sensitivity = xfce_rc_read_int_entry(rc, "sensitivity", default_sensitivity);
            s->bars = xfce_rc_read_int_entry(rc, "bars", default_bars);
            s->bar_width = xfce_rc_read_int_entry(rc, "bar_width", default_bar_width);
            s->bar_spacing = xfce_rc_read_int_entry(rc, "bar_spacing", default_bar_spacing);
            s->bar_shape = xfce_rc_read_int_entry(rc, "bar_shape", default_bar_shape);
            s->bar_caps = xfce_rc_read_int_entry(rc, "bar_caps", default_bar_shape);
            s->max_height = xfce_rc_read_int_entry(rc, "max_height", default_max_height);
            s->lower_cutoff_freq = xfce_rc_read_int_entry(rc, "lower_cutoff_freq", default_lower_cutoff_freq);
            s->higher_cutoff_freq = xfce_rc_read_int_entry(rc, "higher_cutoff_freq", default_higher_cutoff_freq);
            s->sleep_timer = xfce_rc_read_int_entry(rc, "sleep_timer", default_sleep_timer);
            s->input = xfce_rc_read_int_entry(rc, "input", default_input);
            s->source = g_strdup(xfce_rc_read_entry(rc, "source", default_source));
            s->sample_rate = xfce_rc_read_int_entry(rc, "sample_rate", default_sample_rate);
            s->sample_bits = xfce_rc_read_int_entry(rc, "sample_bits", default_sample_bits);
            s->channels = xfce_rc_read_int_entry(rc, "channels", default_channels);
            s->autoconnect = xfce_rc_read_int_entry(rc, "autoconnect", default_autoconnect);
            s->active = xfce_rc_read_int_entry(rc, "active", default_active);
            s->remix = xfce_rc_read_int_entry(rc, "remix", default_remix);
            s->virtual = xfce_rc_read_int_entry(rc, "virtual", default_virtual);
            s->orientation = xfce_rc_read_int_entry(rc, "orientation", default_orientation);
            s->size = xfce_rc_read_int_entry(rc, "size", default_size);
            s->stereo = xfce_rc_read_int_entry(rc, "stereo", default_stereo);
            s->mono_option = xfce_rc_read_int_entry(rc, "mono_option", default_mono_option);
            s->reverse = xfce_rc_read_int_entry(rc, "reverse", default_reverse);
            s->show_idle_bar_heads = xfce_rc_read_int_entry(rc, "show_idle_bar_heads", default_show_idle_bar_heads);
            s->waveform = xfce_rc_read_int_entry(rc, "waveform", default_waveform);
            s->background = g_strdup(xfce_rc_read_entry(rc, "background", default_background));
            s->cap_color = g_strdup(xfce_rc_read_entry(rc, "cap_color", default_cap_color));
            s->foreground1 = g_strdup(xfce_rc_read_entry(rc, "foreground1", default_foreground1));
            s->foreground2 = g_strdup(xfce_rc_read_entry(rc, "foreground2", default_foreground2));
            s->foreground = xfce_rc_read_int_entry(rc, "foreground", default_foreground);
            s->vgradient_colors = g_strdupv(xfce_rc_read_list_entry(rc, "vgradient_colors", ","));
            if (s->vgradient_colors == NULL || 
                    g_strv_length(s->vgradient_colors) < GRADIENT_COLOR_COUNT) {
                s->vgradient_colors = g_strdupv(default_vgradient_colors);
            }
            s->hgradient_colors = g_strdupv(xfce_rc_read_list_entry(rc, "hgradient_colors", ","));
            if (s->hgradient_colors == NULL || 
                    g_strv_length(s->hgradient_colors) < GRADIENT_COLOR_COUNT) {
                s->hgradient_colors = g_strdupv(default_hgradient_colors);
            }
            s->blend_direction = xfce_rc_read_int_entry(rc, "blend_direction", default_blend_direction);
            s->theme = g_strdup(xfce_rc_read_entry(rc, "theme", default_theme));
            s->monstercat = xfce_rc_read_int_entry(rc, "monstercat", default_monstercat);
            s->waves = xfce_rc_read_int_entry(rc, "waves", default_waves);
            s->noise_reduction = xfce_rc_read_int_entry(rc, "noise_reduction", default_noise_reduction);
            s->equalizer = xfce_rc_read_int_entry(rc, "equalizer", default_equalizer);
            gchar **keys = xfce_rc_read_list_entry(rc, "equalizer_keys", ",");
            gint64 value;
            if (keys != NULL) {
                for (int i = 0; i < EQUALIZER_KEY_COUNT && keys[i] != NULL; i++) {
                    value = g_ascii_strtoll(keys[i], NULL, 10);
                    if (errno != ERANGE)
                        s->equalizer_keys[i] = (gdouble)value / 10.0;
                    else
                        s->equalizer_keys[i] = default_equalizer_key;
                }
            }
            g_strfreev(keys);
            s->border = xfce_rc_read_int_entry(rc, "border", default_border);
            s->border_color = g_strdup(xfce_rc_read_entry(rc, "border_color", default_border_color));
            s->margin = xfce_rc_read_int_entry(rc, "margin", default_margin);
            s->padding = xfce_rc_read_int_entry(rc, "padding", default_padding);

            /* cleanup */
            xfce_rc_close(rc);

            /* leave the function, everything went well */
            return;
        }
    }

    /* something went wrong, apply default values */
    DBG("Applying default settings");

    s->framerate = default_framerate;
    s->autosens = default_autosens;
    s->overshoot = default_overshoot;
    s->sensitivity = default_sensitivity;
    s->bars = default_bars;
    s->bar_width = default_bar_width;
    s->bar_spacing = default_bar_spacing;
    s->bar_shape = default_bar_shape;
    s->bar_caps = default_bar_caps;
    s->max_height = default_max_height;
    s->lower_cutoff_freq = default_lower_cutoff_freq;
    s->higher_cutoff_freq = default_higher_cutoff_freq;
    s->max_height = default_max_height;
    s->sleep_timer = default_sleep_timer;
    s->input = default_input;
    s->source = g_strdup(default_source);
    s->sample_rate = default_sample_rate;
    s->sample_bits = default_sample_bits;
    s->channels = default_channels;
    s->autoconnect = default_autoconnect;
    s->active = default_active;
    s->remix = default_remix;
    s->virtual = default_virtual;
    s->orientation = default_orientation;
    s->size = default_size;
    s->stereo = default_stereo;
    s->mono_option = default_mono_option;
    s->reverse = default_reverse;
    s->show_idle_bar_heads = default_show_idle_bar_heads;
    s->waveform = default_waveform;
    s->background = g_strdup(default_background);
    s->cap_color = g_strdup(default_cap_color);
    s->foreground1 = g_strdup(default_foreground1);
    s->foreground2 = g_strdup(default_foreground2);
    s->foreground = default_foreground;
    s->vgradient_colors = g_strdupv(default_vgradient_colors);
    s->hgradient_colors = g_strdupv(default_hgradient_colors);
    s->blend_direction = default_blend_direction;
    s->theme = g_strdup(default_theme);
    s->monstercat = default_monstercat;
    s->waves = default_waves;
    s->noise_reduction = default_noise_reduction;
    s->equalizer = default_equalizer;
    for (int i = 0; i < EQUALIZER_KEY_COUNT; i++)
        s->equalizer_keys[i] = default_equalizer_key;
    s->border = default_border;
    s->border_color = g_strdup(default_border_color);
    s->margin = default_margin;
    s->padding = default_padding;
}

// We can initialize CAVA once the display's size has been fully allocated
static void display_size_allocate(GtkWidget *self, GtkAllocation *allocation, 
        CavaPlugin *c) {
    if (!c->initialized) {
        restyle_display(c);
        init_cava(c);
        start_cava(c);
    }
    config_colors(c);
}

static CavaPlugin *plugin_new(XfcePanelPlugin *plugin) {
    CavaPlugin   *c;
    GtkOrientation  orientation;

    /* allocate memory for the plugin structure */
    c = g_slice_new0(CavaPlugin);

    /* pointer to plugin */
    c->plugin = plugin;

    /* read the user settings */
    plugin_read(c);
    profile_read(c);

    /* get the current orientation */
    orientation = xfce_panel_plugin_get_orientation(plugin);

    /* create some panel widgets */
    c->ebox = gtk_event_box_new();
    gtk_widget_show(c->ebox);

    c->hvbox = gtk_box_new(orientation, 2);
    gtk_widget_show(c->hvbox);
    gtk_container_add(GTK_CONTAINER(c->ebox), c->hvbox);
    gtk_widget_set_name(c->hvbox, "hvbox");

    /* some plugin widgets */
    c->display = gtk_drawing_area_new();
    gtk_widget_show(c->display);
    gtk_container_add(GTK_CONTAINER(c->hvbox), c->display);
    g_signal_connect(G_OBJECT(c->display), "size-allocate", 
            G_CALLBACK(display_size_allocate), c);

    c->css = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(c->css), GTK_STYLE_PROVIDER_PRIORITY_USER);

    return c;
}

static void plugin_free(XfcePanelPlugin *plugin, CavaPlugin *c) {
    GtkWidget *dialog;

    /* check if the dialog is still open. if so, destroy it */
    dialog = g_object_get_data(G_OBJECT(plugin), "dialog");
    if (G_UNLIKELY(dialog != NULL))
        gtk_widget_destroy(dialog);

    /* destroy the panel widgets */
    gtk_widget_destroy(c->hvbox);

    CavaSettings *s = &c->settings;
    if (G_LIKELY(s->source != NULL)) g_free(s->source);
    if (G_LIKELY(s->background != NULL)) g_free(s->background);
    if (G_LIKELY(s->cap_color != NULL)) g_free(s->cap_color);
    if (G_LIKELY(s->foreground1 != NULL)) g_free(s->foreground1);
    if (G_LIKELY(s->foreground2 != NULL)) g_free(s->foreground2);
    if (G_LIKELY(s->border_color != NULL)) g_free(s->border_color);
    if (G_LIKELY(s->theme != NULL)) g_free(s->theme);
    if (G_LIKELY(s->vgradient_colors != NULL)) 
        g_strfreev(s->vgradient_colors);
    if (G_LIKELY(s->hgradient_colors != NULL)) 
        g_strfreev(s->hgradient_colors);
    if (G_LIKELY(s->profile != NULL)) g_free(s->profile);

    /* free the plugin structure */
    g_slice_free(CavaPlugin, c);
}

static void plugin_orientation_changed(XfcePanelPlugin *plugin,
        GtkOrientation orientation, CavaPlugin *c) {
    /* change the orientation of the box */
    gtk_orientable_set_orientation(GTK_ORIENTABLE(c->hvbox), orientation);
}

void resize_display(CavaPlugin *c) {
    CavaSettings *s;
    GtkOrientation orientation;
    int size, plugin_size, adjusted_size, width, height;

    s = &c->settings;
    orientation = xfce_panel_plugin_get_orientation(c->plugin);
    size = s->bars * (s->bar_width + s->bar_spacing) - s->bar_spacing;
    plugin_size = xfce_panel_plugin_get_size(c->plugin);
    adjusted_size = plugin_size - (s->margin * 2) - (s->padding * 2);

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        height = plugin_size;
        c->x_offset = 0;
        if (ORIENT_HORIZONTAL(s->orientation)) {
            width = s->size; //plugin_size * 2;
            c->y_offset = (adjusted_size / 2) - 
                ((s->bars * (s->bar_width + s->bar_spacing)) / 2);
        }
        else {
            width = size;
            c->y_offset = 0;
        }
    }
    else {
        width = plugin_size;
        c->y_offset = 0;
        if (ORIENT_VERTICAL(s->orientation)) {
            height = s->size; //plugin_size * 2;
            c->x_offset = (adjusted_size / 2) - 
                ((s->bars * (s->bar_width + s->bar_spacing)) / 2);
        }
        else {
            height = size;
            c->x_offset = 0;
        }
    }

    gtk_widget_set_size_request(c->display, width, height);
}

void restyle_display(CavaPlugin *c) {
    GdkRGBA rgba;
    CavaSettings *s;
    gchar *background, *border_color;
    gint plugin_size, margin, padding, border;

    s = &c->settings;

    plugin_size = xfce_panel_plugin_get_size(c->plugin);
    margin = s->margin;
    padding = s->padding;
    border = s->border;
    if ((margin + padding + border) * 2 > plugin_size - 2)
        margin = padding = border = 0;

    // CSS can only render alpha channel with rgba(r,g,b,a)
    rgba_parse(&rgba, s->border_color);
    border_color = gdk_rgba_to_string(&rgba);
    rgba_parse(&rgba, s->background);
    background = gdk_rgba_to_string(&rgba);

    // apply new CSS settings
    gchar *css = g_strdup_printf(
            "#hvbox { background: %s; border: %dpx solid %s; margin: %dpx; padding: %dpx; }",
            background, border, border_color, margin, padding);
    gtk_css_provider_load_from_data(c->css, css, -1, NULL);

    // done
    g_free(border_color);
    g_free(background);
    g_free(css);
}

void reset_equalizer(CavaPlugin *c) {
    for (int i = 0; i < EQUALIZER_KEY_COUNT; i++)
        c->settings.equalizer_keys[i] = default_equalizer_key;
}

static gboolean plugin_size_changed(XfcePanelPlugin *plugin, gint size, 
        CavaPlugin *c) {
    GtkOrientation orientation;

    /* get the orientation of the plugin */
    orientation = xfce_panel_plugin_get_orientation(plugin);

    /* set the widget size */
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        gtk_widget_set_size_request(GTK_WIDGET(plugin), -1, size);
    }
    else {
        gtk_widget_set_size_request(GTK_WIDGET(plugin), size, -1);
    }

    resize_display(c);

    /* we handled the orientation */
    return TRUE;
}

static void enable_plugin_cb(GtkCheckMenuItem *widget, CavaPlugin *c) {
    c->enabled = gtk_check_menu_item_get_active(widget);
    if (c->enabled) {
        start_cava(c);
    }
}

static void plugin_construct(XfcePanelPlugin *plugin) {
    CavaPlugin *c;

    /* setup transation domain */
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    /* create the plugin */
    c = plugin_new(plugin);

    /* add the ebox to the panel */
    gtk_container_add(GTK_CONTAINER(plugin), c->ebox);

    /* show the panel's right-click menu on this ebox */
    xfce_panel_plugin_add_action_widget(plugin, c->ebox);

    /* connect plugin signals */
    g_signal_connect(G_OBJECT(plugin), "free-data",
            G_CALLBACK(plugin_free), c);

    g_signal_connect(G_OBJECT(plugin), "save",
            G_CALLBACK(plugin_save), c);

    g_signal_connect(G_OBJECT(plugin), "size-changed",
            G_CALLBACK(plugin_size_changed), c);

    g_signal_connect(G_OBJECT(plugin), "orientation-changed",
            G_CALLBACK(plugin_orientation_changed), c);

    /* show the configure menu item and connect signal */
    xfce_panel_plugin_menu_show_configure(plugin);
    g_signal_connect(G_OBJECT(plugin), "configure-plugin",
            G_CALLBACK(plugin_configure), c);

    /* show the about menu item and connect signal */
    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect(G_OBJECT(plugin), "about",
            G_CALLBACK(plugin_about), NULL);

    GtkMenuItem *enable_plugin = GTK_MENU_ITEM(gtk_check_menu_item_new_with_label(_("Enabled")));
    xfce_panel_plugin_menu_insert_item(plugin, enable_plugin);
    g_signal_connect(G_OBJECT(enable_plugin), "toggled", G_CALLBACK(enable_plugin_cb), c);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(enable_plugin), TRUE);
    gtk_widget_show(GTK_WIDGET(enable_plugin));
}

/*  $Id$
 *
 *  Copyright (C) 2012 John Doo <john@foo.org>
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

#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include <libxfce4panel/libxfce4panel.h>
#include "cava/input/common.h"

G_BEGIN_DECLS

enum input_method {
    //INPUT_FIFO,
    //INPUT_PORTAUDIO,
    INPUT_PIPEWIRE,
    //INPUT_ALSA,
    INPUT_PULSE,
    //INPUT_SNDIO,
    //INPUT_OSS,
    //INPUT_JACK,
    //INPUT_SHMEM,
    //INPUT_WINSCAP,
    //INPUT_MAX,
};

enum mono_option { LEFT, RIGHT, AVERAGE };

enum orientation {
    ORIENT_BOTTOM,
    ORIENT_TOP,
    ORIENT_LEFT,
    ORIENT_RIGHT,
    ORIENT_SPLIT_H,
    ORIENT_SPLIT_V,
};

enum foreground {
    FG_STYLE_ONE_COLOR = 0,
    FG_STYLE_TWO_COLORS = 1,
    FG_STYLE_VGRADIENT = 2,
    FG_STYLE_HGRADIENT = 3,
};

#define ORIENT_SPLIT(o) \
    (((o) == ORIENT_SPLIT_H) || ((o) == ORIENT_SPLIT_V))

#define ORIENT_VERTICAL(o) \
    (((o) == ORIENT_BOTTOM) || ((o) == ORIENT_TOP) || ((o) == ORIENT_SPLIT_H))

#define ORIENT_HORIZONTAL(o) \
    (((o) == ORIENT_LEFT) || ((o) == ORIENT_RIGHT) || ((o) == ORIENT_SPLIT_V))

enum bar_shape {
    BAR_SHAPE_RECTANGLE,
    BAR_SHAPE_OBLONG,
};

#define EQUALIZER_MAX       2.0
#define EQUALIZER_KEY_COUNT 10
#define GRADIENT_COLOR_COUNT 8

typedef struct {
    /* general */
    gint    framerate;
    gint    autosens;
    gint    overshoot;
    gint    sensitivity;
    gint    bars;
    gint    bar_width;
    gint    bar_spacing;
    gint    bar_shape;
    gint    bar_caps;
    gint    max_height;
    gint    lower_cutoff_freq;
    gint    higher_cutoff_freq;
    gint    sleep_timer;
    /* input */
    gint    input;
    gchar   *source;
    gint    sample_rate;
    gint    sample_bits;
    gint    channels;
    gint    autoconnect;
    gint    active;
    gint    remix;
    gint    virtual;
    /* output */
    gint    orientation;
    gint    stereo;
    gint    mono_option;
    gint    reverse;
    gint    show_idle_bar_heads;
    gint    waveform;
    /* color */
    gchar   *background;
    gint    foreground;
    gchar   *foreground1;
    gchar   *foreground2;
    gchar   *cap_color;
    gchar   **vgradient_colors;
    gchar   **hgradient_colors;
    gint    blend_direction;
    gchar   *theme;
    /* smoothing */
    gint    monstercat;
    gint    waves;
    gint    noise_reduction;
    gint    equalizer;
    gdouble equalizer_keys[EQUALIZER_KEY_COUNT];
    gint    border;
    gchar   *border_color;
    gint    margin;
    gint    padding;
    gchar   *profile; // profile filename
} CavaSettings;

typedef struct {
    /* profile */
    GtkWidget *profile;
    GtkWidget *new_profile;
    GtkWidget *del_profile;
    GtkWidget *ren_profile;
    GtkWidget *input;
    /* general */
    GtkWidget *framerate;
    GtkWidget *sensitivity;
    GtkWidget *bars;
    GtkWidget *bar_width;
    GtkWidget *bar_spacing;
    GtkWidget *bar_shape;
    GtkWidget *bar_caps;
    GtkWidget *lower_cutoff_freq;
    GtkWidget *higher_cutoff_freq;
    GtkWidget *sleep_timer;
    /* input */
    /* output */
    GtkWidget *orientation;
    GtkWidget *stereo;
    /* color */
    GtkWidget *background;
    GtkWidget *foreground1;
    GtkWidget *foreground2;
    GtkWidget *foreground;
    GtkWidget *cap_color;
    GtkWidget *vgradient_colors[GRADIENT_COLOR_COUNT];
    GtkWidget *hgradient_colors[GRADIENT_COLOR_COUNT];
    GtkWidget *mirror_vgradient;
    GtkWidget *mirror_hgradient;
    GtkWidget *box_scolors;
    GtkWidget *box_vgrad;
    GtkWidget *box_hgrad;
    /* smoothing */
    GtkWidget *monstercat;
    GtkWidget *waves;
    GtkWidget *noise_reduction;
    GtkWidget *equalizer;
    GtkWidget *equalizer_keys[EQUALIZER_KEY_COUNT];
    GtkWidget *border;
    GtkWidget *border_color;
    GtkWidget *margin;
    GtkWidget *padding;
} SettingWidgets;

typedef struct {
    int       *bars;
    double    *caps;
    int       *previous_frame;
    float     *bars_left;
    float     *bars_right;
    double    *cava_out;
    float     *bars_raw;
    int       number_of_bars;
    int       raw_number_of_bars;
    int       output_channels;
} CavaData;

/* plugin structure */
typedef struct {
    XfcePanelPlugin *plugin;

    /* panel widgets */
    GtkWidget   *ebox;
    GtkWidget   *hvbox;
    GtkWidget   *display;

    /* plugin settings */
    gboolean        enabled;
    GtkWidget       *settings_dialog;
    CavaSettings    settings;
    SettingWidgets  widgets;
    GtkCssProvider  *css;

    /* cava */
    CavaData            data;
    struct cava_plan    *plan;
    struct audio_data   audio;
    pthread_t audio_thread;
    cairo_pattern_t     *foreground;
    cairo_pattern_t     *cap_color;

    // offsets for centering rotated bars
    gint x_offset;
    gint y_offset;

    /* cava data */
    gboolean    initialized;
    gint        timeout_id;
    gint        profile_count;
} CavaPlugin;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rectangle;

void init_cava(CavaPlugin *cava);
void start_cava(CavaPlugin *cava);
void stop_cava(CavaPlugin *cava);
void config_cava(CavaPlugin *cava);
void free_cava(CavaPlugin *cava);
void free_audio(CavaPlugin *cava);
void resize_display(CavaPlugin *cava);
void restyle_display(CavaPlugin *cava);
void config_colors(CavaPlugin *cava);
void rgba_parse(GdkRGBA *c, gchar *spec);
void reset_equalizer(CavaPlugin *cava);
void plugin_read(CavaPlugin *c);
void plugin_save(CavaPlugin *c);
void profile_read(CavaPlugin *c);
void profile_save(CavaPlugin *c);
gchar *get_profile_path(gchar *profile);
gchar *get_profile_dir(void);

G_END_DECLS

#endif /* !__SAMPLE_H__ */

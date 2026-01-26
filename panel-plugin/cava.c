#include <math.h>

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include "cava/util.h"
#include "cava/input/pulse.h"
#include "cava/input/pipewire.h"
#include "cava/cavacore.h"
#include "plugin.h"

#ifdef __GNUC__
// curses.h or other sources may already define
#undef GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif

void config_colors(CavaPlugin *c) {
    GdkRGBA fg;
    CavaSettings *s;
    GtkAllocation alloc;
    double offset, step;
    gint i, x0, y0, x1, y1;
    GtkWidget *display = c->display;
    gtk_widget_get_allocation(display, &alloc);
    s = &c->settings;
    if (s->gradient) {
        switch (s->orientation) {
            case ORIENT_BOTTOM:
            case ORIENT_SPLIT_H:
                x0 = x1 = y1 = 0;
                y0 = alloc.height;
                break;
            case ORIENT_TOP:
                x0 = x1 = y0 = 0;
                y1 = alloc.height;
                break;
            case ORIENT_LEFT:
            case ORIENT_SPLIT_V:
                x0 = y0 = y1 = 0;
                x1 = alloc.width;
                break;
            case ORIENT_RIGHT:
                x1 = y0 = y1 = 0;
                x0 = alloc.width;
                break;
            default:
                exit(EXIT_FAILURE);
        }
        c->foreground = cairo_pattern_create_linear(x0, y0, x1, y1);
        if (s->orientation == ORIENT_SPLIT_H || 
                s->orientation == ORIENT_SPLIT_V) {
            offset = step = 0.0625;
            for (int n = 0; n < GRADIENT_COLOR_COUNT * 2; n++) {
                if (n >= GRADIENT_COLOR_COUNT)
                    i = n - GRADIENT_COLOR_COUNT;
                else
                    i = GRADIENT_COLOR_COUNT - 1 - n;
                rgba_parse(&fg, s->gradient_colors[i]);
                cairo_pattern_add_color_stop_rgba(c->foreground, offset, 
                        fg.red, fg.green, fg.blue, fg.alpha);
                offset += step;
            }
        }
        else {
            offset = step = 0.125;
            for (int n = 0; n < GRADIENT_COLOR_COUNT; n++) {
                rgba_parse(&fg, s->gradient_colors[n]);
                cairo_pattern_add_color_stop_rgba(c->foreground, offset, 
                        fg.red, fg.green, fg.blue, fg.alpha);
                offset += step;
            }
        }
    }
    else if (s->horizontal_gradient) {
        c->foreground = cairo_pattern_create_linear(0, 0, alloc.width, 0);
        offset = 0.125;
        for (int n = 0; n < GRADIENT_COLOR_COUNT; n++) {
            rgba_parse(&fg, s->horizontal_gradient_colors[n]);
            cairo_pattern_add_color_stop_rgba(c->foreground, offset, 
                    fg.red, fg.green, fg.blue, fg.alpha);
            offset += 0.125;
        }
    }
    else {
        // solid color
        rgba_parse(&fg, s->foreground);
        c->foreground = cairo_pattern_create_rgba(
                fg.red, fg.green, fg.blue, fg.alpha);
    }
}

static gboolean draw_cava(GtkWidget *display, cairo_t *cr, CavaPlugin *c) {
    CavaData *d;
    CavaSettings *s;
    GtkAllocation alloc;
    gint x, y, w, h, rx, ry, r, bar_width, bar_spacing;

    // bar size
    d = &c->data;
    s = &c->settings;
    bar_width = s->bar_width;
    bar_spacing = s->bar_spacing;
    gtk_widget_get_allocation(display, &alloc);

    // data
    int *bars = d->bars;
    int number_of_bars = d->number_of_bars;

    // foreground color
    cairo_set_source(cr, c->foreground);

    // draw the bars
    for (int n = 0; n < number_of_bars; n++) {
        if (bars[n] == 0)
            continue;
        x = y = w = h = 0;
        switch (s->orientation) {
            case ORIENT_BOTTOM:
                x = n * (bar_width + bar_spacing);
                y = alloc.height - bars[n];
                w = bar_width;
                h = bars[n];
                break;
            case ORIENT_TOP:
                x = n * (bar_width + bar_spacing);
                y = 0;
                w = bar_width;
                h = bars[n];
                break;
            case ORIENT_SPLIT_H:
                x = n * (bar_width + bar_spacing);
                y = (alloc.height / 2) - (bars[n] / 2);
                w = bar_width;
                h = bars[n];
                break;
            case ORIENT_LEFT:
                x = 0;
                y = n * (bar_width + bar_spacing);
                w = bars[n];
                h = bar_width;
                break;
            case ORIENT_RIGHT:
                x = alloc.width - bars[n];
                y = n * (bar_width + bar_spacing);
                w = bars[n];
                h = bar_width;
                break;
            case ORIENT_SPLIT_V:
                x = (alloc.width / 2) - (bars[n] / 2);
                y = n * (bar_width + bar_spacing);
                w = bars[n];
                h = bar_width;
                break;
        }
        x += c->x_offset;
        y += c->y_offset;
        if (s->bar_shape == BAR_SHAPE_RECTANGLE) {
            cairo_rectangle(cr, x, y, w, h);
        }
        else if (s->bar_shape == BAR_SHAPE_OBLONG) {
            if (ORIENT_HORIZONTAL(s->orientation)) {
                r = MIN(w / 2, bar_width / 2);
                rx = MIN(w / 2, bar_width / 2);
                ry = bar_width / 2;
            }
            else {
                r = MIN(h / 2, bar_width / 2);
                rx = bar_width / 2;
                ry = MIN(h / 2, bar_width / 2);
            }
            cairo_new_sub_path(cr);
            cairo_arc(cr, x + rx, y + ry, r, M_PI, 3 * M_PI / 2);
            cairo_arc(cr, x + w - rx, y + ry, r, 3 * M_PI / 2, 2 * M_PI);
            cairo_arc(cr, x + w - rx, y + h - ry, r, 0, M_PI / 2);
            cairo_arc(cr, x + rx, y + h - ry, r, M_PI / 2, M_PI);
            cairo_close_path(cr);
        }
        cairo_fill(cr);
    }

    return FALSE;
}

static float *monstercat_filter(float *bars, int number_of_bars, int waves, 
        double monstercat, int height) {
    int z;
    int m_y, de;
    float height_normalizer = 1.0;
    monstercat = (100.0 - (monstercat / 3.0)) / 100.0;
    if (height > 1000) {
        height_normalizer = height / 912.76;
    }
    if (waves > 0) {
        for (z = 0; z < number_of_bars; z++) { // waves
            bars[z] = bars[z] / 1.25;
            // if (bars[z] < 1) bars[z] = 1;
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                bars[m_y] = max(
                        bars[z] - height_normalizer * pow(de, 2), bars[m_y]);
            }
            for (m_y = z + 1; m_y < number_of_bars; m_y++) {
                de = m_y - z;
                bars[m_y] = max(
                        bars[z] - height_normalizer * pow(de, 2), bars[m_y]);
            }
        }
    }
    else if (monstercat > 0) {
        for (z = 0; z < number_of_bars; z++) {
            // if (bars[z] < 1)bars[z] = 1;
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                bars[m_y] = max(
                        bars[z] / pow(monstercat * 1.5, de), bars[m_y]);
            }
            for (m_y = z + 1; m_y < number_of_bars; m_y++) {
                de = m_y - z;
                bars[m_y] = max(
                        bars[z] / pow(monstercat * 1.5, de), bars[m_y]);
            }
        }
    }
    return bars;
}

static gboolean exec_cava(CavaPlugin *c) {
    CavaData *d = &c->data;
    CavaSettings *s = &c->settings;
    struct audio_data *audio = &c->audio;

    // ignore silence
    static gint sleep_counter = 0;
    gboolean silence = TRUE;
    if (s->sleep_timer > 0) {
        for (int n = 0; n < audio->input_buffer_size; n++) {
            if (audio->cava_in[n]) {
                sleep_counter = 0;
                silence = FALSE;
                break;
            }
        }
        if (silence)
            sleep_counter += 1000 / s->framerate;
        if (sleep_counter >= s->sleep_timer * 1000) {
            sleep_counter = s->sleep_timer * 1000;
            return TRUE;
        }
    }

    // data
    int *bars = d->bars;
    int *previous_frame = d->previous_frame;
    float *bars_left = d->bars_left;
    float *bars_right = d->bars_right;
    double *cava_out = d->cava_out;
    float *bars_raw = d->bars_raw;
    int number_of_bars = d->number_of_bars;
    int raw_number_of_bars = d->raw_number_of_bars;
    int output_channels = d->output_channels;

    // set size
    GtkAllocation alloc;
    gtk_widget_get_allocation(c->display, &alloc);
    int dimension_value = alloc.height;
    if (s->orientation == ORIENT_LEFT || s->orientation == ORIENT_RIGHT || 
            s->orientation == ORIENT_SPLIT_V)
        dimension_value = alloc.width;
    if (dimension_value < 2)
        return TRUE;

    // execute
    pthread_mutex_lock(&audio->lock);
    double sensitivity = (double)s->sensitivity / 100;
    if (s->waveform) {
        for (int n = 0; n < audio->samples_counter; n++) {
            for (int i = number_of_bars - 1; i > 0; i--) {
                cava_out[i] = cava_out[i - 1];
            }
            if (audio->channels == 2) {
                cava_out[0] =
                    sensitivity * (audio->cava_in[n] / 2 + 
                            audio->cava_in[n + 1] / 2);
                n++;
            }
            else {
                cava_out[0] = sensitivity * audio->cava_in[n];
            }
        }
    }
    else {
        cava_execute(
                audio->cava_in, audio->samples_counter, cava_out, c->plan);
    }
    if (audio->samples_counter > 0) {
        audio->samples_counter = 0;
    }
    pthread_mutex_unlock(&audio->lock);

    // sensitivity
    for (int n = 0; n < raw_number_of_bars; n++) {
        if (!s->waveform) {
            cava_out[n] *= sensitivity;
        } 
        else {
            if (cava_out[n] > 1.0)
                sensitivity *= 0.999;
            else
                sensitivity *= 1.00001;
            if (s->orientation != ORIENT_SPLIT_H)
                cava_out[n] = (cava_out[n] + 1.0) / 2.0;
        }
        cava_out[n] *= dimension_value;
        if (s->orientation == ORIENT_SPLIT_H ||
                s->orientation == ORIENT_SPLIT_V) {
            //cava_out[n] /= 2;
        }
        if (s->waveform) {
            bars_raw[n] = cava_out[n];
        }
    }

    double eq_ratio = 0;
    double eq_key;
    if (!s->waveform) {
        // equalizer
        if (s->equalizer && (number_of_bars / output_channels > 0)) {
            eq_ratio = (double)(EQUALIZER_KEY_COUNT / 
                    ((double)(number_of_bars / output_channels)));
        }
        if (audio->channels == 2) {
            for (int n = 0; n < number_of_bars / output_channels; n++) {
                if (s->equalizer) {
                    eq_key = s->equalizer_keys[(int)floor(((double)n) * eq_ratio)];
                    cava_out[n] *= eq_key;
                }
                bars_left[n] = cava_out[n];
            }
            for (int n = 0; n < number_of_bars / output_channels; n++) {
                if (s->equalizer) {
                    eq_key = s->equalizer_keys[(int)floor(((double)n) * eq_ratio)];
                    cava_out[n + number_of_bars / output_channels] *= eq_key;
                }
                bars_right[n] = cava_out[n + number_of_bars / output_channels];
            }
        }
        else {
            for (int n = 0; n < number_of_bars; n++) {
                if (s->equalizer) {
                    eq_key = s->equalizer_keys[(int)floor(((double)n) * eq_ratio)];
                    cava_out[n] *= eq_key;
                }
                bars_raw[n] = cava_out[n];
            }
        }
        // process [filter]
        if (s->monstercat) {
            if (audio->channels == 2) {
                bars_left =
                    monstercat_filter(
                            bars_left, number_of_bars / output_channels,
                            s->waves, s->monstercat, dimension_value);
                bars_right =
                    monstercat_filter(
                            bars_right, number_of_bars / output_channels,
                            s->waves, s->monstercat, dimension_value);
            }
            else {
                bars_raw = monstercat_filter(bars_raw, number_of_bars, 
                        s->waves, s->monstercat, dimension_value);
            }
        }
        if (audio->channels == 2) {
            if (s->stereo) {
                // mirroring stereo channels
                for (int n = 0; n < number_of_bars; n++) {
                    if (n < number_of_bars / 2) {
                        if (s->reverse) {
                            bars_raw[n] = bars_left[n];
                        }
                        else {
                            bars_raw[n] = bars_left[number_of_bars / 2 - n - 1];
                        }
                    }
                    else {
                        if (s->reverse) {
                            bars_raw[n] = bars_right[number_of_bars - n - 1];
                        }
                        else {
                            bars_raw[n] = bars_right[n - number_of_bars / 2];
                        }
                    }
                }
            }
            else {
                // stereo mono output
                for (int n = 0; n < number_of_bars; n++) {
                    if (s->reverse) {
                        if (s->mono_option == AVERAGE) {
                            bars_raw[number_of_bars - n - 1] =
                                (bars_left[n] + bars_right[n]) / 2;
                        }
                        else if (s->mono_option == LEFT) {
                            bars_raw[number_of_bars - n - 1] = bars_left[n];
                        }
                        else if (s->mono_option == RIGHT) {
                            bars_raw[number_of_bars - n - 1] = bars_right[n];
                        }
                    }
                    else {
                        if (s->mono_option == AVERAGE) {
                            bars_raw[n] = (bars_left[n] + bars_right[n]) / 2;
                        }
                        else if (s->mono_option == LEFT) {
                            bars_raw[n] = bars_left[n];
                        }
                        else if (s->mono_option == RIGHT) {
                            bars_raw[n] = bars_right[n];
                        }
                    }
                }
            }
        }
    }
    int re_paint = 0;
    for (int n = 0; n < number_of_bars; n++) {
        bars[n] = bars_raw[n];
        // show idle bar heads
        if (bars[n] < 1 && s->waveform == 0 && s->show_idle_bar_heads == 1)
            bars[n] = 1;
        if (bars[n] != previous_frame[n])
            re_paint = 1;
    }
    if (re_paint) {
        gtk_widget_queue_draw(c->display);
        memcpy(previous_frame, bars, number_of_bars * sizeof(int));
    }

    // update data
    d->number_of_bars = number_of_bars;
    d->raw_number_of_bars = raw_number_of_bars;
    d->output_channels = output_channels;

    return TRUE;
}

void free_cava(CavaPlugin *c) {
    DBG(".");
    CavaData *d = &c->data;
    g_source_remove(d->timeout_id);
    cava_destroy(c->plan);
    cairo_pattern_destroy(c->foreground);
    free(c->plan);
    free(d->bars_left);
    free(d->bars_right);
    free(d->cava_out);
    free(d->bars);
    free(d->bars_raw);
    free(d->previous_frame);
}

static void init_audio(CavaPlugin *c) {
    DBG(".");
    CavaSettings *s = &c->settings;
    struct audio_data *audio = &c->audio;
    audio->source = malloc(1 + strlen(s->source));
    strcpy(audio->source, s->source);
    audio->format = -1;
    audio->rate = 0;
    audio->samples_counter = 0;
    audio->channels = 2;
    audio->IEEE_FLOAT = 0;
    audio->autoconnect = 0;
    audio->input_buffer_size = BUFFER_SIZE * audio->channels;
    audio->cava_buffer_size = 16384;
    audio->cava_in = (double *)malloc(
            audio->cava_buffer_size * sizeof(double));
    memset(audio->cava_in, 0, sizeof(int) * audio->cava_buffer_size);
    audio->threadparams = 0;
    audio->terminate = 0;
    pthread_t p_thread;
    int timeout_counter = 0;
    struct timespec timeout_timer = {.tv_sec = 0, .tv_nsec = 1000000};
    int thr_id GCC_UNUSED;
    pthread_mutex_init(&audio->lock, NULL);
    switch (s->method) {
        case INPUT_PULSE:
            audio->format = 16;
            audio->rate = 44100;
            if (strcmp(audio->source, "auto") == 0) {
                getPulseDefaultSink((void *)audio);
            }
            thr_id = pthread_create(&p_thread, NULL, input_pulse, 
                    (void *)audio);
            break;
        case INPUT_PIPEWIRE:
            audio->format = s->sample_bits;
            audio->rate = s->sample_rate;
            audio->channels = s->channels;
            audio->active = s->active;
            audio->remix = s->remix;
            audio->virtual_node = s->virtual;
            thr_id = pthread_create(&p_thread, NULL, input_pipewire, 
                    (void *)audio);
            break;
        default:
            exit(EXIT_FAILURE); // Can't happen.
    }
    timeout_counter = 0;
    while (TRUE) {
        nanosleep(&timeout_timer, NULL);
        pthread_mutex_lock(&audio->lock);
        if ((audio->threadparams == 0) && (audio->format != -1) && 
                (audio->rate != 0))
            break;
        pthread_mutex_unlock(&audio->lock);
        timeout_counter++;
        if (timeout_counter > 5000) {
            fprintf(stderr, "could not get rate and/or format, problems with " 
                    "audio thread? quitting...\n");
            exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_unlock(&audio->lock);
    if ((guint)s->higher_cutoff_freq > audio->rate / 2) {
        fprintf(stderr,
                "higher cutoff frequency can't be higher than sample rate / 2\n" 
                "higher cutoff frequency is set to: %d, got sample rate: %d\n",
                s->higher_cutoff_freq, audio->rate);
        exit(EXIT_FAILURE);
    }
}

void config_cava(CavaPlugin *c) {
    DBG(".");
    CavaData *d = &c->data;
    CavaSettings *s = &c->settings;
    struct audio_data *audio = &c->audio;

    // data
    double *cava_out;
    int *bars, *previous_frame;
    float *bars_left, *bars_right, *bars_raw;
    int number_of_bars, raw_number_of_bars, output_channels, timeout_id;

    // force stereo if only one channel is available
    if (s->stereo && audio->channels == 1)
        s->stereo = 0;
    output_channels = 1;
    if (s->stereo && s->bars > 1)
        output_channels = 2;

    // number of bars
    number_of_bars = s->bars;
    if (s->stereo)
        number_of_bars = s->bars / output_channels * output_channels;
    raw_number_of_bars = (number_of_bars / output_channels) * audio->channels;
    if (s->waveform) {
        raw_number_of_bars = number_of_bars;
    }
    double noise_reduction = (double)s->noise_reduction / 100.0;
    struct cava_plan *plan = c->plan = 
        cava_init(number_of_bars / output_channels, audio->rate, 
                audio->channels, s->autosens, noise_reduction,
                s->lower_cutoff_freq, s->higher_cutoff_freq);
    if (plan->status == -1) {
        fprintf(stderr, "Error initializing cava . %s", plan->error_message);
        exit(EXIT_FAILURE);
    }
    if (plan->input_buffer_size != audio->cava_buffer_size) {
        pthread_mutex_lock(&audio->lock);
        audio->cava_buffer_size = plan->input_buffer_size;
        free(audio->cava_in);
        audio->cava_in = (double *)malloc(
                audio->cava_buffer_size * sizeof(double));
        memset(audio->cava_in, 0, sizeof(double) * audio->cava_buffer_size);
        pthread_mutex_unlock(&audio->lock);
    }
    bars_left = (float *)malloc(
            number_of_bars / output_channels * sizeof(float));
    bars_right = (float *)malloc(
            number_of_bars / output_channels * sizeof(float));
    memset(bars_left, 0, sizeof(float) * number_of_bars / output_channels);
    memset(bars_right, 0, sizeof(float) * number_of_bars / output_channels);
    bars = (int *)malloc(number_of_bars * sizeof(int));
    bars_raw = (float *)malloc(number_of_bars * sizeof(float));
    previous_frame = (int *)malloc(number_of_bars * sizeof(int));
    cava_out = (double *)malloc(number_of_bars * audio->channels / 
            output_channels * sizeof(double));
    memset(bars, 0, sizeof(int) * number_of_bars);
    memset(bars_raw, 0, sizeof(float) * number_of_bars);
    memset(previous_frame, 0, sizeof(int) * number_of_bars);
    memset(cava_out, 0, sizeof(double) * number_of_bars * audio->channels / 
            output_channels);
    // checking if audio thread has exited unexpectedly
    pthread_mutex_lock(&audio->lock);
    if (audio->terminate == 1) {
        fprintf(stderr, "Audio thread exited unexpectedly. %s\n", 
                audio->error_message);
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&audio->lock);
    config_colors(c);

    int timeout = 1000 / c->settings.framerate;
    timeout_id = g_timeout_add(timeout, (GSourceFunc)exec_cava, c);

    // set data
    d->bars = bars;
    d->previous_frame = previous_frame;
    d->bars_left = bars_left;
    d->bars_right = bars_right;
    d->cava_out = cava_out;
    d->bars_raw = bars_raw;
    d->number_of_bars = number_of_bars;
    d->raw_number_of_bars = raw_number_of_bars;
    d->output_channels = output_channels;
    d->timeout_id = timeout_id;
}

void init_cava(CavaPlugin *c) {
    DBG(".");
    init_audio(c);
    config_cava(c);
    c->initialized = TRUE;
    g_signal_connect(G_OBJECT(c->display), "draw", G_CALLBACK(draw_cava), c);
}

/*
  Hatari - floppy_sound.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "floppy_sound.h"

/* ------------------------------------------------------------------ */
/* Built-in synthetic click                                             */
/* ------------------------------------------------------------------ */
#define BUILTIN_SAMPLES 2425   /* ~55ms @ 44100Hz */
static int16_t builtin_pcm[BUILTIN_SAMPLES * 2];
static int     builtin_generated = 0;

static void generate_builtin(void)
{
    int i;
    for (i = 0; i < BUILTIN_SAMPLES; i++)
    {
        float t  = (float)i / 44100.0f;
        float s  = 0.0f;

        /* --- 1. mechanical thud: 120 Hz, decays in ~12ms --- */
        {
            float env = expf(-t / 0.012f);
            s += sinf(2.0f * 3.14159265f * 120.0f * t) * env * 0.55f;
        }

        /* --- 2. stepper buzz: 800 Hz, onset 2ms, decays by 20ms --- */
        {
            float onset = (t > 0.002f) ? 1.0f : (t / 0.002f);
            float env   = onset * expf(-t / 0.008f);
            float freq  = 800.0f + 60.0f * sinf(2.0f * 3.14159265f * 180.0f * t);
            s += sinf(2.0f * 3.14159265f * freq * t) * env * 0.30f;
        }

        /* --- 3. head seek chirp: 2400->700 Hz sweep over 25ms --- */
        {
            float chirp_len = 0.025f;
            float env = (t < chirp_len)
                        ? expf(-t / 0.018f)
                        : 0.0f;
            float freq = 2400.0f - (2400.0f - 700.0f) * (t / chirp_len);
            if (t < chirp_len)
                s += sinf(2.0f * 3.14159265f * freq * t) * env * 0.20f;
        }

        /* --- 4. broadband rattle: pseudo-noise, 5-15ms --- */
        {
            float t2 = t - 0.005f;
            if (t2 > 0.0f && t2 < 0.012f)
            {
                float env = expf(-t2 / 0.005f);
                float n = sinf(2.0f * 3.14159265f * 3700.0f * t2)
                        * sinf(2.0f * 3.14159265f * 2300.0f * t2)
                        * sinf(2.0f * 3.14159265f *  970.0f * t2);
                s += n * env * 0.18f;
            }
        }

        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t out = (int16_t)(s * 28000.0f);
        builtin_pcm[i * 2    ] = out;
        builtin_pcm[i * 2 + 1] = out;
    }
    builtin_generated = 1;
}

/* ------------------------------------------------------------------ */
/* Sample storage                                                       */
/* ------------------------------------------------------------------ */
static int16_t *sample_data   = NULL;
static int      sample_frames = 0;
static int      sample_is_external = 0;

static int play_pos[2] = { -1, -1 };
static int prev_led[2] = { 0, 0 };

static int floppy_enabled = 1;
static int floppy_volume  = 200;   /* 0-256 */

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */
void floppy_sound_init(const char *system_dir)
{
    char path[512];
    FILE *f;
    long  size;

    if (!builtin_generated)
        generate_builtin();

    if (system_dir)
    {
        snprintf(path, sizeof(path), "%s/floppy.raw", system_dir);
        f = fopen(path, "rb");
        if (f)
        {
            fseek(f, 0, SEEK_END);
            size = ftell(f);
            rewind(f);
            if (size > 0)
            {
                sample_data = (int16_t *)malloc((size_t)size);
                if (sample_data)
                {
                    size_t nread = fread(sample_data, 1, (size_t)size, f);
                    if ((long)nread == size)
                    {
                        sample_frames      = (int)(size / (2 * sizeof(int16_t)));
                        sample_is_external = 1;
                    }
                    else
                    {
                        free(sample_data);
                        sample_data = NULL;
                    }
                }
            }
            fclose(f);
        }
    }

    if (!sample_data)
    {
        sample_data        = builtin_pcm;
        sample_frames      = BUILTIN_SAMPLES;
        sample_is_external = 0;
    }

    play_pos[0] = play_pos[1] = -1;
    prev_led[0] = prev_led[1] = 0;
}

void floppy_sound_set_enabled(int enabled)
{
    floppy_enabled = enabled;
}

void floppy_sound_set_volume(int vol)
{
    floppy_volume = vol;
}

void floppy_sound_update_leds(int leda, int ledb)
{
    if (leda && !prev_led[0])
        play_pos[0] = 0;
    if (ledb && !prev_led[1])
        play_pos[1] = 0;

    prev_led[0] = leda;
    prev_led[1] = ledb;
}

void floppy_sound_mix(int16_t *buf, int frames)
{
    int d, i;

    if (!floppy_enabled || !sample_data)
        return;

    for (d = 0; d < 2; d++)
    {
        if (play_pos[d] < 0)
            continue;

        for (i = 0; i < frames && play_pos[d] < sample_frames; i++, play_pos[d]++)
        {
            int l = (int)buf[i * 2    ] + ((int)sample_data[play_pos[d] * 2    ] * floppy_volume >> 8);
            int r = (int)buf[i * 2 + 1] + ((int)sample_data[play_pos[d] * 2 + 1] * floppy_volume >> 8);
            buf[i * 2    ] = (l >  32767) ?  32767 : (l < -32768) ? (int16_t)-32768 : (int16_t)l;
            buf[i * 2 + 1] = (r >  32767) ?  32767 : (r < -32768) ? (int16_t)-32768 : (int16_t)r;
        }

        if (play_pos[d] >= sample_frames)
            play_pos[d] = -1;
    }
}

void floppy_sound_free(void)
{
    if (sample_is_external && sample_data)
        free(sample_data);
    sample_data        = NULL;
    sample_frames      = 0;
    sample_is_external = 0;
}

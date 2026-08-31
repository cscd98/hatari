/*
  Hatari - floppy_sound.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_RETRO_FLOPPY_SOUND_H
#define HATARI_RETRO_FLOPPY_SOUND_H

#include <stdint.h>

void floppy_sound_init(const char *system_dir);
void floppy_sound_set_enabled(int enabled);
void floppy_sound_set_volume(int vol);   /* 0-256, default 200 */
void floppy_sound_update_leds(int leda, int ledb);
void floppy_sound_mix(int16_t *buf, int frames);
void floppy_sound_free(void);

#endif /* HATARI_RETRO_FLOPPY_SOUND_H */

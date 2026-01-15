#ifndef VC_AUDIO_IO_H
#define VC_AUDIO_IO_H

#include <gtk-4.0/gtk/gtk.h>
#include <soundio/soundio.h>
#include <stdbool.h>

void    VcAudioIoInit(const char *vcPath);
bool    VcAudioIoIsInitialized();
bool    VcAudioIoTogglePlayback();
void    VcAudioIoFinalize();
void    VcAudioIoReset();

#endif // VC_AUDIO_IO_H
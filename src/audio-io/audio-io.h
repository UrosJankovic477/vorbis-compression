#ifndef VC_AUDIO_IO_H
#define VC_AUDIO_IO_H

#include <gtk-4.0/gtk/gtk.h>
#include <soundio/soundio.h>
#include <stdbool.h>

void    VcAudioIoInit(const char *vcPath);
bool    VcAudioIoIsInitialized();
bool    VcAudioIoTogglePlayback();
void    VcAudioIoSetVolume(double _volume);
void    VcAudioIoSeek(double timestamp);
void    VcAudioIoFinalize();
double  VcAudioIoGetTimestamp();
double  VcAudioIoGetDuration();

#endif // VC_AUDIO_IO_H
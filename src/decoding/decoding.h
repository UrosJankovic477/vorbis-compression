#ifndef VC_DECODING_H
#define VC_DECODING_H

#include <gtk-4.0/gtk/gtkmediastream.h>

void VC_StartAudioThread(GtkMediaStream *stream);
void VC_StopAudioThread(GtkMediaStream *stream);


#endif // VC_DECODING_H
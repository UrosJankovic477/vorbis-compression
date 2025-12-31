#ifndef VC_DECODING_H
#define VC_DECODING_H

#include <gtk-4.0/gtk/gtkmediastream.h>

void VcStartAudioThread(GtkMediaStream *stream);
void VcStopAudioThread(GtkMediaStream *stream);


#endif // VC_DECODING_H
#ifndef VC_OGG_STREAM_H
#define VC_OGG_STREAM_H

#include <gtk-4.0/gtk/gtkmediastream.h>

G_BEGIN_DECLS

#define VC_OGG_STREAM (vc_ogg_stream_get_type())

G_DECLARE_FINAL_TYPE(VcOggStream, vc_ogg_stream, VC_, OGG_STREAM, GtkMediaStream)

VcOggStream *vc_ogg_stream_new(void);

G_END_DECLS

void vc_ogg_stream_play(VcOggStream *self);
void vc_ogg_stream_pause(VcOggStream *self);
void vc_ogg_stream_seek(VcOggStream *self, gint64 timestamp);
void vc_ogg_stream_set_volume(VcOggStream *self, double volume);

#endif // VC_OGG_STREAM_H
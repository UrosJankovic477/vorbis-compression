#include "ogg-stream.h"
#include "../decoding/decoding.h"

struct _VcOggStream {
    GtkMediaStream  *parent;
    gboolean        playing;
    gint64          duration;
    gint64          timestamp;
    double          volume;  

};

G_DEFINE_FINAL_TYPE(VcOggStream, vc_ogg_stream, GTK_MEDIA_STREAM)

static void vc_ogg_stream_class_init(VcOggStreamClass *klass)
{
    GtkMediaStreamClass *mediaStreamClass = GTK_MEDIA_STREAM_CLASS(klass);
    mediaStreamClass->play          = vc_ogg_stream_play;
    mediaStreamClass->pause         = vc_ogg_stream_pause;
    mediaStreamClass->seek          = vc_ogg_stream_seek;
}

static void vc_ogg_stream_init(VcOggStream *self)
{
    self->playing   = FALSE;
    self->duration  = 0;
    self->timestamp = 0;
    self->volume    = 1.0;
}

VcOggStream *vc_ogg_stream_new(void)
{
    return g_object_new(VC_OGG_STREAM, NULL);
}

void vc_ogg_stream_play(VcOggStream *self) 
{
    if (self->playing)
    {
        return; 
    }
    
    self->playing = TRUE;
    gtk_media_stream_set_playing(GTK_MEDIA_STREAM(self), TRUE);
    VcStartAudioThread(GTK_MEDIA_STREAM(self));
}

void vc_ogg_stream_pause(VcOggStream *self) 
{
    if (!self->playing)
    {
        return; 
    }
    
    self->playing = FALSE;
    gtk_media_stream_set_playing(GTK_MEDIA_STREAM(self), FALSE);
    VcStopAudioThread(GTK_MEDIA_STREAM(self));
}

void vc_ogg_stream_seek(VcOggStream *self, gint64 timestamp) 
{

}

void vc_ogg_stream_set_volume(VcOggStream *self, double volume) 
{
    
}

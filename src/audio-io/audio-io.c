#include "audio-io.h"
#include <vorbis/vorbisfile.h>
#include <string.h>
#include <glib.h>
#include <math.h>

static OggVorbis_File   vf;
static GThread          *audioIoThread              = NULL;
static GThread          *decoderThread              = NULL;
static gboolean         initialized                 = false;
static int              bitstream                   = 0;
static gint             events                      = 0;

typedef enum
{
    VC_AUDIO_IO_EOS     = 1,
    VC_AUDIO_IO_PAUSE   = 2,
    VC_AUDIO_IO_PLAY    = 4,
    VC_AUDIO_IO_STOP    = 8,
    VC_AUDIO_IO_RESET   = 16,

} VcAudioIoEvents;

static struct SoundIoRingBuffer *rb = NULL;

gpointer VcDecodeCallback(gpointer data)
{
    char buffer[4092];
    char *writePtr;
    struct SoundIoOutStream *outstream = (struct SoundIoOutStream *)data; 
    while (!(g_atomic_int_get(&events) & VC_AUDIO_IO_STOP))
    {
        if ((g_atomic_int_get(&events) & VC_AUDIO_IO_RESET))
        {
            writePtr = soundio_ring_buffer_write_ptr(rb);
            int bytesToSet = soundio_ring_buffer_free_count(rb);
            memset(writePtr, 0, bytesToSet);
            soundio_outstream_pause(outstream, true);
            g_atomic_int_set(&events, (VC_AUDIO_IO_PAUSE));
            ov_time_seek(&vf, 0);
            soundio_ring_buffer_clear(rb);
            soundio_outstream_clear_buffer(outstream);
        }
        
        int bytesAvailable = soundio_ring_buffer_free_count(rb);
        int length = 4092 < bytesAvailable ? 4092 : bytesAvailable;
        long nBytes = ov_read(&vf, buffer, length, 0, 2, 1, &bitstream);
        if (nBytes == 0)
        {
            g_atomic_int_or(&events, VC_AUDIO_IO_EOS);
        }
        else if (nBytes > 0)
        {
            writePtr = soundio_ring_buffer_write_ptr(rb);
            if (writePtr == NULL)
            {
                return NULL;
            }
            
            memcpy(writePtr, buffer, nBytes);
            if (nBytes < bytesAvailable)
            {
                memset(writePtr + nBytes, 0, bytesAvailable - nBytes);
            }

            soundio_ring_buffer_advance_write_ptr(rb, nBytes);
        }
    }

    return NULL;
}

static void VcAudioIoWriteCallback
(
    struct SoundIoOutStream *outstream, 
    int frameCountMin, 
    int frameCountMax
)
{
    const struct SoundIoChannelLayout *layout = &outstream->layout;
    struct SoundIoChannelArea *areas;
    int framesLeft = frameCountMax;
    int error;

    while(framesLeft > 0 && !((g_atomic_int_get(&events) & VC_AUDIO_IO_EOS))) {

        int frameCount = framesLeft;

        if ((error = soundio_outstream_begin_write(outstream, &areas, &frameCount))) 
        {
            g_error(soundio_strerror(error));
            return;
        }

        if (!frameCount)
        {
            break;
        }

        int available = soundio_ring_buffer_fill_count(rb);
        int requested = frameCount * layout->channel_count * 2;
        char *readPtr = soundio_ring_buffer_read_ptr(rb);
        
        int16_t *pcm = (int16_t *)readPtr;

        for (size_t i = 0; i < available / 2; i++)
        {
            int ch = i % layout->channel_count;
            int16_t *ptr = (int16_t *)(areas[ch].ptr + i / layout->channel_count * areas[ch].step);
            *ptr = pcm[i];
        }

        if (available < requested)
        {
            for (size_t ch = 0; ch < layout->channel_count; ch++)
            {
                int16_t *ptr = (int16_t *)(areas[ch].ptr + available / 2 * areas[ch].step);
                memset(ptr, 0, (requested - available) / 2);
            }
        }        

        int bytesToWrite = available < requested ? available : requested;
        soundio_ring_buffer_advance_read_ptr(rb, bytesToWrite);
        
        if ((error = soundio_outstream_end_write(outstream))) 
        {
            g_error(soundio_strerror(error));
            return;
        }
        
        framesLeft -= frameCount;
    }
}

gpointer VcInitAudioIoCallback(gpointer data) 
{
    int error;
    g_atomic_int_set(&events, 0);
    
    vorbis_info *info = (vorbis_info *)data;
    struct SoundIo *soundio = soundio_create();
    if (!soundio) {
        g_error("failed to create sound io sturct");
        return NULL;
    }

    if ((error = soundio_connect(soundio))) {
        g_error(soundio_strerror(error));
        return NULL;
    }

    soundio_flush_events(soundio);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) {
        g_error("no output device found");
        return NULL;
    }

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device) {
        g_error("couldn't get output device");
        return NULL;
    }

    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Output Audio Device: %s", device->name);

    struct SoundIoOutStream *outstream  = soundio_outstream_create(device);
    outstream->format                   = SoundIoFormatS16LE;
    outstream->write_callback           = VcAudioIoWriteCallback;
    outstream->sample_rate              = info->rate;
    outstream->bytes_per_frame          = 2 * info->channels;
    outstream->layout                   = *soundio_channel_layout_get_default(info->channels);

    if ((error = soundio_outstream_open(outstream))) 
    {
        g_error(soundio_strerror(error));
        return NULL;
    }

    if (outstream->layout_error)
    {
        g_error(soundio_strerror(outstream->layout_error));
        return NULL;
    }

    g_atomic_int_set(&events, (VC_AUDIO_IO_PAUSE));

    rb = soundio_ring_buffer_create(soundio, info->rate * 0.2);
    decoderThread = g_thread_new("decoder", VcDecodeCallback, outstream);
        
    if ((error = soundio_outstream_start(outstream))) {
        soundio_strerror(outstream->layout_error);
        return NULL;
    }

    initialized = true;

    while (true)
    {
        soundio_flush_events(soundio);
        int ev = g_atomic_int_get(&events);
        if (ev & VC_AUDIO_IO_STOP)
        {
            break;
        }

        if (ev & VC_AUDIO_IO_PAUSE)
        {
            soundio_outstream_pause(outstream, true);
        }

        if (ev & VC_AUDIO_IO_PLAY)
        {
            soundio_outstream_pause(outstream, false);
        }
    }

    g_atomic_int_set(&events, 0);
    soundio_ring_buffer_destroy(rb);
    soundio_outstream_clear_buffer(outstream);
    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    
    return NULL;
}

void VcAudioIoInit(const char *vcPath)
{
    ov_fopen(vcPath, &vf);
    audioIoThread = g_thread_new("audio-io", VcInitAudioIoCallback, ov_info(&vf, -1));
}

void VcAudioIoFinalize() 
{
    g_atomic_int_set(&events, VC_AUDIO_IO_STOP);

    if (decoderThread)
    {
        g_thread_join(decoderThread);
    }

    if (audioIoThread)
    {
        g_thread_join(audioIoThread);
    }
    
    ov_clear(&vf);
    
    initialized = false;
    audioIoThread = NULL;
    decoderThread = NULL;
}

void VcAudioIoReset()
{
    g_atomic_int_or(&events, VC_AUDIO_IO_RESET);
}

bool VcAudioIoIsInitialized() 
{ 
    return initialized; 
}

bool VcAudioIoTogglePlayback() 
{
    int ev = g_atomic_int_get(&events);
    bool playing = false;
    if (ev & VC_AUDIO_IO_PAUSE)
    {
        g_atomic_int_xor(&events, VC_AUDIO_IO_PAUSE);
        g_atomic_int_or(&events, VC_AUDIO_IO_PLAY);
        playing = true;
    }
    else if (ev & VC_AUDIO_IO_PLAY)
    {
        g_atomic_int_xor(&events, VC_AUDIO_IO_PLAY);
        g_atomic_int_or(&events, VC_AUDIO_IO_PAUSE);
        playing = false;
    }
    
    return playing;
}

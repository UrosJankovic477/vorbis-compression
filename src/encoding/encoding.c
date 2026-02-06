#include "encoding.h"
#include "../gui/log-view.h"
#include <stdio.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <string.h>
#include <time.h>

typedef enum
{
    VC_WAVE_FORMAT_PCM          = 1,
    VC_WAVE_FORMAT_IEEE_FLOAT   = 3,
    VC_WAVE_FORMAT_ALAW         = 6,
    VC_WAVE_FORMAT_MULAW        = 7,
    VC_WAVE_FORMAT_EXTENSIBLE   = 0xfffe

} VcWaveFormat;

typedef struct
{
    uint16_t    wFormatTag;
    uint8_t     padding[14];

} VcWaveSubFormat;

typedef struct
{
    uint16_t        wFormatTag;
	uint16_t        nChannels; 	
	uint32_t        nSamplesPerSec;
	uint32_t        nAvgBytesPerSec;
	uint16_t        nBlockAlign;
	uint16_t        wBitsPerSample;

} VcWaveHeaderCommon;

typedef struct 
{
    // Input and output files
    GFileInputStream    *pInfile;
    GFileOutputStream   *pOutfile;
    
    // Input file data
    uint32_t            nDataSize;
    VcWaveHeaderCommon  common;

    // Ogg vorbis structures
    vorbis_info         vi;
    vorbis_dsp_state    dsp;
    vorbis_comment      comment;
    vorbis_block        block;
    ogg_stream_state    stream;
    ogg_packet          packet;
    ogg_page            page;

} VcEncodingContext;

static VcEncodingContext vcCtx = { 0 };
static uint8_t vcReadBuffer[VC_BUFFER_SIZE];
static GThread *encoderThread = NULL;

int VcReadHeader(GtkTextView *logView)
{
    VcLogViewWriteLine(logView, "Reading header...");

    GError *error = NULL;
    size_t nBytes;
    g_seekable_seek(G_SEEKABLE(vcCtx.pInfile), 20, G_SEEK_SET, NULL, &error);
    if (error != NULL)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        VcLogViewWriteLine(logView, error->message);
        return -1;
    }

    nBytes = g_input_stream_read(G_INPUT_STREAM(vcCtx.pInfile), &vcCtx.common, 16, NULL, &error);
    if (error != NULL || nBytes < 16)
    {
        if (error)
        {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
            VcLogViewWriteLine(logView, error->message);
        }

        else
        {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Failed to read header");
            VcLogViewWriteLine(logView, "Failed to read header");
        }
        
        
        return -1;
    }

    if (vcCtx.common.wFormatTag == VC_WAVE_FORMAT_EXTENSIBLE)
    {
        g_seekable_seek(G_SEEKABLE(vcCtx.pInfile), 8, G_SEEK_CUR, NULL, &error);
        VcWaveSubFormat subFormat;
        nBytes = g_input_stream_read(G_INPUT_STREAM(vcCtx.pInfile), &subFormat, 16, NULL, &error);
        if (error != NULL)
        {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
            VcLogViewWriteLine(logView, error->message);
            return -1;
        }

        vcCtx.common.wFormatTag = subFormat.wFormatTag;
    }
    
    else if (vcCtx.common.wFormatTag == VC_WAVE_FORMAT_PCM)
    {
        g_seekable_seek(G_SEEKABLE(vcCtx.pInfile), 40, G_SEEK_SET, NULL, &error);
    }

    else
    {
        VcLogViewWriteLine(logView, "Format not recognized");
        return -1;
        
    }
    
    
    if (error != NULL)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        VcLogViewWriteLine(logView, error->message);
        return -1;
    }

    nBytes = g_input_stream_read(G_INPUT_STREAM(vcCtx.pInfile), &vcCtx.nDataSize, 4, NULL, &error);
    if (nBytes < 4)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        VcLogViewWriteLine(logView, error->message);
        return -1;
    }

    VcLogViewWriteLine(logView, "Header processed:");
    VcLogViewWriteLine(logView, "Number of channels: %d", vcCtx.common.nChannels);
    VcLogViewWriteLine(logView, "Bits per sample: %d", vcCtx.common.wBitsPerSample);
    VcLogViewWriteLine(logView, "Sample rate: %d", vcCtx.common.nSamplesPerSec);

    return 0;
}

void VcWriteBufferPCM(size_t n_bytes)
{
    float **channels = vorbis_analysis_buffer(&vcCtx.dsp, VC_BUFFER_SIZE);
    uint16_t bytesPerSample = vcCtx.common.wBitsPerSample / 8;
    uint16_t stride = bytesPerSample * vcCtx.common.nChannels;
    for(size_t i = 0; i < n_bytes / stride; i++)
    {
        for (size_t ch = 0; ch < vcCtx.common.nChannels; ch++)
        {
            switch (bytesPerSample)
            {
                case 1:
                {
                    uint8_t sample = (uint8_t) (vcReadBuffer[i * stride + ch]);
                    channels[ch][i] = sample / 256.0f;
                    break;
                }

                case 2:
                {
                    uint16_t sample = (((uint16_t) vcReadBuffer[i * stride + ch * bytesPerSample]) 
                    | ((uint16_t) vcReadBuffer[i * stride + ch * bytesPerSample + 1] << 8));
                    channels[ch][i] = ((int16_t) sample) / 32768.0f;
                    break;
                }

                case 3:
                {
                    uint32_t sample = (((uint32_t) vcReadBuffer[i * stride + ch * bytesPerSample]) 
                    | ((uint32_t) vcReadBuffer[i * stride + ch * bytesPerSample + 1] << 8)
                    | ((uint32_t) vcReadBuffer[i * stride + ch * bytesPerSample + 2] << 16));
                    sample &= 0x00ffffff;
                    sample |= 0xff000000 * ((sample & 0x00800000) != 0);  // checks if 24th bit is 1 and ors highest byte with 0xff if true
                    channels[ch][i] = ((int32_t) sample) / 8388608.0f;
                    break;
                }

                case 4:
                {
                    uint32_t sample = (((uint32_t) vcReadBuffer[i * stride + ch * bytesPerSample]) 
                    | ((uint32_t) vcReadBuffer[i * stride + ch * bytesPerSample + 1] << 8)
                    | ((uint32_t) vcReadBuffer[i * stride + ch * bytesPerSample + 2] << 16)
                    | ((uint32_t) vcReadBuffer[i * stride + ch * bytesPerSample + 3] << 24));
                    channels[ch][i] = ((int32_t) sample) / 2147483648.0;
                    break;
                }

                default: break;
            }
            
        }
        
    }
    vorbis_analysis_wrote(&vcCtx.dsp, n_bytes / stride);
}

void VcWriteBufferFloat(size_t n_bytes)
{
    float **channels = vorbis_analysis_buffer(&vcCtx.dsp, VC_BUFFER_SIZE);
    uint16_t bytesPerSample = vcCtx.common.wBitsPerSample / 8;
    uint16_t stride = bytesPerSample * vcCtx.common.nChannels;

    for(size_t i = 0; i < n_bytes / stride; i++)
    {
        for (size_t ch = 0; ch < vcCtx.common.nChannels; ch++)
        {
            if (bytesPerSample == 4)
            {
                float sample = ((float) vcReadBuffer[i * stride + ch * bytesPerSample]);
                channels[ch][i] = sample;
            }

            else if (bytesPerSample == 8)
            {
                double sample = ((double) vcReadBuffer[i * stride + ch * bytesPerSample]);
                channels[ch][i] = sample;
            }
            
            
             
        }
    }

}

void VcWriteBufferALaw(size_t n_bytes)
{

}

void VcWriteBufferMuLaw(size_t n_bytes)
{
    
}

void VcWriteBuffer(size_t n_bytes)
{
    switch (((VcWaveFormat) vcCtx.common.wFormatTag))
    {
        case VC_WAVE_FORMAT_PCM:
        {
            VcWriteBufferPCM(n_bytes);
            break;
        }

        case VC_WAVE_FORMAT_IEEE_FLOAT:
        {
            VcWriteBufferFloat(n_bytes);
            break;
        }

        case VC_WAVE_FORMAT_ALAW:
        {
            VcWriteBufferALaw(n_bytes);
            break;
        }

        case VC_WAVE_FORMAT_MULAW:
        {
            VcWriteBufferMuLaw(n_bytes);
            break;
        }

        default: break;
    }
}

void VcEncoderFinalize(VcEncodeOptions *options)
{
    ogg_stream_clear(&vcCtx.stream);
    vorbis_block_clear(&vcCtx.block);
    vorbis_dsp_clear(&vcCtx.dsp);
    vorbis_comment_clear(&vcCtx.comment);
    vorbis_info_clear(&vcCtx.vi);

    g_input_stream_close(options->pInFileStream, NULL, NULL);

    g_main_context_invoke(NULL, options->cbOnFinished, options);
}

GThread *VcGetEncoderThread() 
{ 
    return encoderThread; 
}

int VcEncodeCallback(VcEncodeOptions *options)
{
    vcCtx.pInfile = options->pInFileStream;
    vcCtx.pOutfile = options->pOutFileStream;

    GError *error = NULL;

    int status;

    status = VcReadHeader(options->pLogView);

    if (status < 0)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Failed to parse header");
        VcEncoderFinalize(options);
        return -1;
    }

    srand(time(NULL));
    status = ogg_stream_init(&vcCtx.stream, rand());
    if (status < 0)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Couldn't initialize vorbis stream");
        VcEncoderFinalize(options);
        return -1;
    }

    vorbis_info_init(&vcCtx.vi);
    status = vorbis_encode_init_vbr(&vcCtx.vi, vcCtx.common.nChannels, vcCtx.common.nSamplesPerSec, options->fDesiredQuality);
    if (status < 0)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Couldn't initialize vorbis encoding engine");
        VcEncoderFinalize(options);
        return -1;
    }

    status = vorbis_analysis_init(&vcCtx.dsp, &vcCtx.vi);
    if (status < 0)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Couldn't initialize vorbis analysis engine");
        VcEncoderFinalize(options);
        return -1;
    }
    
    vorbis_comment_init(&vcCtx.comment);
    ogg_packet header_packet, comment_packet, code_packet;
    status = vorbis_analysis_headerout(&vcCtx.dsp, &vcCtx.comment, &header_packet, &comment_packet, &code_packet);
    if (status < 0)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Failed to write header");
        VcEncoderFinalize(options);
        return -1;
    }

    status = vorbis_block_init(&vcCtx.dsp, &vcCtx.block);
    if (status < 0)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Couldn't initialize vorbis block structure");
        VcEncoderFinalize(options);
        return -1;
    }

    ogg_stream_packetin(&vcCtx.stream, &header_packet);
    ogg_stream_packetin(&vcCtx.stream, &comment_packet);
    ogg_stream_packetin(&vcCtx.stream, &code_packet);

    while (true)
    {
        int result = ogg_stream_flush(&vcCtx.stream, &vcCtx.page);
        if(result == 0) 
        {
            break;
        }
        g_output_stream_write(G_OUTPUT_STREAM(vcCtx.pOutfile), vcCtx.page.header, vcCtx.page.header_len, NULL, &error);
        g_output_stream_write(G_OUTPUT_STREAM(vcCtx.pOutfile), vcCtx.page.body, vcCtx.page.body_len, NULL, &error);
        if (error != NULL)
        {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
            VcEncoderFinalize(options);
            return -1;
        }
    }
    
    bool eos = false;

    while (!eos)
    {
        size_t n_bytes =  g_input_stream_read(G_INPUT_STREAM(vcCtx.pInfile), vcReadBuffer, VC_BUFFER_SIZE, NULL, &error);
        if (error != NULL)
        {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
            VcEncoderFinalize(options);
            return -1;
        }

        if (n_bytes == 0)
        {
            vorbis_analysis_wrote(&vcCtx.dsp, 0);
        }
        else
        {
            VcWriteBuffer(n_bytes);
        }

        while ((status = vorbis_analysis_blockout(&vcCtx.dsp, &vcCtx.block)) > 0)
        {
            status = vorbis_analysis(&vcCtx.block, NULL);
            if (status < 0)
            {
                g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Analysis error: %d", status);
                VcEncoderFinalize(options);
                return -1;
            }
            status = vorbis_bitrate_addblock(&vcCtx.block);
            if (status < 0)
            {
                g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Couldn't add block: %d", status);
                VcEncoderFinalize(options);
                return -1;
            }

            while ((status = vorbis_bitrate_flushpacket(&vcCtx.dsp, &vcCtx.packet)) > 0)
            {
                status = ogg_stream_packetin(&vcCtx.stream, &vcCtx.packet);
                if (status < 0)
                {
                    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Failed to read packet from stream: %d", status);
                    VcEncoderFinalize(options);
                    return -1;
                }
                while (!eos)
                {
                    int result = ogg_stream_pageout(&vcCtx.stream, &vcCtx.page);
                    if (result == 0)
                    {
                        break;
                    }
                    
                    g_output_stream_write(G_OUTPUT_STREAM(vcCtx.pOutfile), vcCtx.page.header, vcCtx.page.header_len, NULL, &error);
                    g_output_stream_write(G_OUTPUT_STREAM(vcCtx.pOutfile), vcCtx.page.body, vcCtx.page.body_len, NULL, &error);
                    if (error != NULL)
                    {
                        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
                        VcEncoderFinalize(options);
                        return -1;
                    }

                    if(ogg_page_eos(&vcCtx.page))
                    {
                        eos = true;
                    }
                }
            }
            if (status < 0)
            {
                g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Failed to flush packet: %d", status);
                VcEncoderFinalize(options);
                return -1;
            }

        }
        if (status < 0)
        {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Couldn't write block: %d", status);
            VcEncoderFinalize(options);
            return -1;
        }
        
    }

    VcEncoderFinalize(options);
    
    return 0;
}

GThread *VcEncode(VcEncodeOptions *options)
{
    return encoderThread = g_thread_new("encoder", VcEncodeCallback, options);
}
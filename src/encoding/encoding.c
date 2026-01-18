#include "encoding.h"
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

} VC_WaveFormat;

typedef struct
{
    uint16_t        wFormatTag;
	uint16_t        nChannels; 	
	uint32_t        nSamplesPerSec;
	uint32_t        nAvgBytesPerSec;
	uint16_t        nBlockAlign;
	uint16_t        wBitsPerSample;

} VC_WaveHeaderCommon;

typedef struct 
{
    // Input and output files
    GFileInputStream    *pInfile;
    GFileOutputStream   *pOutfile;
    
    // Input file data
    uint32_t            nDataSize;
    VC_WaveHeaderCommon common;

    // Ogg vorbis structures
    vorbis_info         vi;
    vorbis_dsp_state    dsp;
    vorbis_comment      comment;
    vorbis_block        block;
    ogg_stream_state    stream;
    ogg_packet          packet;
    ogg_page            page;

} VC_EncodingContext;

static VC_EncodingContext vc_ctx = { 0 };
static uint8_t vc_read_buffer[VC_BUFFER_SIZE];

int VC_ReadHeader()
{
    GError *error = NULL;
    g_seekable_seek(G_SEEKABLE(vc_ctx.pInfile), 20, G_SEEK_SET, NULL, &error);
    if (error != NULL)
    {
        g_error(error->message);
        return -1;
    }
    size_t nBytes = g_input_stream_read(G_INPUT_STREAM(vc_ctx.pInfile), &vc_ctx.common, 16, NULL, &error);
    if (error != NULL || nBytes < 16)
    {
        g_error(error->message);
        return -1;
    }

    g_seekable_seek(G_SEEKABLE(vc_ctx.pInfile), 40, G_SEEK_SET, NULL, &error);
    if (error != NULL)
    {
        g_error(error->message);
        return -1;
    }

    nBytes = g_input_stream_read(G_INPUT_STREAM(vc_ctx.pInfile), &vc_ctx.nDataSize, 4, NULL, &error);
    if (nBytes < 4)
    {
        g_error(error->message);
        return -1;
    }

    return 0;
}

void VC_WriteBufferPCM(size_t n_bytes)
{
    float **channels = vorbis_analysis_buffer(&vc_ctx.dsp, VC_BUFFER_SIZE);
    uint16_t bytesPerSample = vc_ctx.common.wBitsPerSample / 8;
    uint16_t stride = bytesPerSample * vc_ctx.common.nChannels;
    for(size_t i = 0; i < n_bytes / stride; i++)
    {
        for (size_t ch = 0; ch < vc_ctx.common.nChannels; ch++)
        {
            switch (bytesPerSample)
            {
                case 1:
                {
                    uint8_t sample = (uint8_t) (vc_read_buffer[i * stride + ch]);
                    channels[ch][i] = sample / 256.0f;
                    break;
                }

                case 2:
                {
                    uint16_t sample = (((uint16_t) vc_read_buffer[i * stride + ch * bytesPerSample]) 
                    | ((uint16_t) vc_read_buffer[i * stride + ch * bytesPerSample + 1] << 8));
                    channels[ch][i] = ((int16_t) sample) / 32768.0f;
                    break;
                }

                case 3:
                {
                    uint32_t sample = (((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample]) 
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 1] << 8)
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 2] << 16));
                    sample &= 0x00ffffff;
                    sample |= 0xff000000 * ((sample & 0x00800000) != 0);  // checks if 24th bit is 1 and ors highest byte with 0xff if true
                    channels[ch][i] = ((int32_t) sample) / 8388608.0f;
                    break;
                }

                case 4:
                {
                    uint32_t sample = (((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample]) 
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 1] << 8)
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 2] << 16)
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 3] << 24));
                    channels[ch][i] = ((int32_t) sample) / 2147483648.0;
                    break;
                }

                default: break;
            }
            
        }
        
    }
    vorbis_analysis_wrote(&vc_ctx.dsp, n_bytes / stride);
}

void VC_WriteBufferFloat(size_t n_bytes)
{
    
}

void VC_WriteBufferALaw(size_t n_bytes)
{

}

void VC_WriteBufferMuLaw(size_t n_bytes)
{
    
}

void VC_WriteBuffer(size_t n_bytes)
{
    switch (((VC_WaveFormat) vc_ctx.common.wFormatTag))
    {
        case VC_WAVE_FORMAT_PCM:
        {
            VC_WriteBufferPCM(n_bytes);
            break;
        }

        case VC_WAVE_FORMAT_IEEE_FLOAT:
        {
            VC_WriteBufferFloat(n_bytes);
            break;
        }

        case VC_WAVE_FORMAT_ALAW:
        {
            VC_WriteBufferALaw(n_bytes);
            break;
        }

        case VC_WAVE_FORMAT_MULAW:
        {
            VC_WriteBufferMuLaw(n_bytes);
            break;
        }

        default: break;
    }
}

int VcEncode(VcEncodeOptions options)
{
    vc_ctx.pInfile = options.pInFileStream;
    vc_ctx.pOutfile = options.pOutFileStream;

    GError *error = NULL;

    int status;

    status = VC_ReadHeader();

    if (status < 0)
    {
        g_error("Failed to parse header");
        return -1;
    }

    srand(time(NULL));
    status = ogg_stream_init(&vc_ctx.stream, rand());
    if (status < 0)
    {
        g_error("Couldn't initialize vorbis stream");
        return -1;
    }

    vorbis_info_init(&vc_ctx.vi);
    status = vorbis_encode_init_vbr(&vc_ctx.vi, vc_ctx.common.nChannels, vc_ctx.common.nSamplesPerSec, options.fDesiredQuality);
    if (status < 0)
    {
        g_error("Couldn't initialize vorbis encoding engine");
        return -1;
    }

    status = vorbis_analysis_init(&vc_ctx.dsp, &vc_ctx.vi);
    if (status < 0)
    {
        g_error("Couldn't initialize vorbis analysis engine");
        return -1;
    }
    
    vorbis_comment_init(&vc_ctx.comment);
    ogg_packet header_packet, comment_packet, code_packet;
    status = vorbis_analysis_headerout(&vc_ctx.dsp, &vc_ctx.comment, &header_packet, &comment_packet, &code_packet);
    if (status < 0)
    {
        g_error("Failed to write header");
        return -1;
    }

    status = vorbis_block_init(&vc_ctx.dsp, &vc_ctx.block);
    if (status < 0)
    {
        g_error("Couldn't initialize vorbis block structure");
        return -1;
    }

    ogg_stream_packetin(&vc_ctx.stream, &header_packet);
    ogg_stream_packetin(&vc_ctx.stream, &comment_packet);
    ogg_stream_packetin(&vc_ctx.stream, &code_packet);

    while (true)
    {
        int result = ogg_stream_flush(&vc_ctx.stream, &vc_ctx.page);
        if(result == 0) 
        {
            break;
        }
        g_output_stream_write(G_OUTPUT_STREAM(vc_ctx.pOutfile), vc_ctx.page.header, vc_ctx.page.header_len, NULL, &error);
        g_output_stream_write(G_OUTPUT_STREAM(vc_ctx.pOutfile), vc_ctx.page.body, vc_ctx.page.body_len, NULL, &error);
        if (error != NULL)
        {
            g_error(error->message);
            return -1;
        }
    }
    
    bool eos = false;

    while (!eos)
    {
        size_t n_bytes =  g_input_stream_read(G_INPUT_STREAM(vc_ctx.pInfile), vc_read_buffer, VC_BUFFER_SIZE, NULL, &error);
        if (error != NULL)
        {
            g_error(error->message);
            return -1;
        }

        if (n_bytes == 0)
        {
            vorbis_analysis_wrote(&vc_ctx.dsp, 0);
        }
        else
        {
            VC_WriteBuffer(n_bytes);
        }

        while ((status = vorbis_analysis_blockout(&vc_ctx.dsp, &vc_ctx.block)) > 0)
        {
            status = vorbis_analysis(&vc_ctx.block, NULL);
            if (status < 0)
            {
                g_error("Analysis error: %d", status);
                return -1;
            }
            status = vorbis_bitrate_addblock(&vc_ctx.block);
            if (status < 0)
            {
                g_error("Couldn't add block: %d", status);
                return -1;
            }

            while ((status = vorbis_bitrate_flushpacket(&vc_ctx.dsp, &vc_ctx.packet)) > 0)
            {
                status = ogg_stream_packetin(&vc_ctx.stream, &vc_ctx.packet);
                if (status < 0)
                {
                    g_error("Failed to read packet from stream: %d", status);
                    return -1;
                }
                while (!eos)
                {
                    int result = ogg_stream_pageout(&vc_ctx.stream, &vc_ctx.page);
                    if (result == 0)
                    {
                        break;
                    }
                    
                    g_output_stream_write(G_OUTPUT_STREAM(vc_ctx.pOutfile), vc_ctx.page.header, vc_ctx.page.header_len, NULL, &error);
                    g_output_stream_write(G_OUTPUT_STREAM(vc_ctx.pOutfile), vc_ctx.page.body, vc_ctx.page.body_len, NULL, &error);
                    if (error != NULL)
                    {
                        g_error(error->message);
                        return -1;
                    }

                    if(ogg_page_eos(&vc_ctx.page))
                    {
                        eos = true;
                    }
                }
            }
            if (status < 0)
            {
                g_error("Failed to flush packet: %d", status);
                return -1;
            }

        }
        if (status < 0)
        {
            g_error("Couldn't write block: %d", status);
            return -1;
        }
        
    }

    ogg_stream_clear(&vc_ctx.stream);
    vorbis_block_clear(&vc_ctx.block);
    vorbis_dsp_clear(&vc_ctx.dsp);
    vorbis_comment_clear(&vc_ctx.comment);
    vorbis_info_clear(&vc_ctx.vi);

    g_input_stream_close(options.pInFileStream, NULL, NULL);
    
    return 0;
}

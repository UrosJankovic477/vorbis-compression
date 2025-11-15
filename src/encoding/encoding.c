#include "encoding.h"
#include <stdio.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <string.h>
#include <time.h>
#include "../logging/logging.h"

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
    FILE                *pInfile;
    FILE                *pOutfile;
    
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

    // Is state valid
    bool                bValid;

} VC_EncodingContext;

static VC_EncodingContext vc_ctx = { 0 };
static uint8_t vc_read_buffer[VC_RB_SIZE];

static int VC_OpenOutputFile(const char *filepath)
{
    vc_ctx.pOutfile = fopen(filepath, "w");
    vc_ctx.bValid = vc_ctx.pOutfile != NULL;
    return vc_ctx.bValid;
}

int VC_OpenInputFile(const char *filepath)
{
    char message[VC_LOG_MSG_MAXLEN] = { 0 };
    snprintf(message, VC_LOG_MSG_MAXLEN, "Opening input file %s", filepath);
    VC_PushLogMessage(message, VC_LOG_INFO);

    vc_ctx.pInfile = fopen(filepath, "r");
    vc_ctx.bValid = vc_ctx.pInfile != NULL;
    return vc_ctx.bValid;
}


static void VC_ReadHeader()
{
    fseek(vc_ctx.pInfile, 20, SEEK_SET);
    size_t nBytes = fread(&vc_ctx.common, 1, 16, vc_ctx.pInfile);
    if (nBytes < 16)
    {
        vc_ctx.bValid = false;
        return;
    }

    fseek(vc_ctx.pInfile, 40, SEEK_SET);
    nBytes = fread(&vc_ctx.nDataSize, 1, 4, vc_ctx.pInfile);
    if (nBytes < 4)
    {
        vc_ctx.bValid = false;
        return;
    }
        
}

void VC_WriteBufferPCM(size_t n_bytes)
{
    float **channels = vorbis_analysis_buffer(&vc_ctx.dsp, VC_RB_SIZE);
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
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 1] << 8))
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 2] << 16);
                    sample |= 0xff000000 * (sample >> 23);  // checks if 24th bit is 1 and ors highest byte with 0xff if true
                    channels[ch][i] = ((int32_t) sample) / 8388608.0f;
                    break;
                }

                case 4:
                {
                    uint16_t sample = (((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample]) 
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 1] << 8)
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 2] << 16)
                    | ((uint32_t) vc_read_buffer[i * stride + ch * bytesPerSample + 3] << 24));
                    channels[ch][i] = ((int32_t) sample) / 2147483648.0f;
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


bool VC_IsStateValid()
{
    return vc_ctx.bValid;
}

int VC_Encode(VC_EncodeOptions options)
{
    VC_OpenInputFile(options.sInFilePath);

    if (!vc_ctx.bValid)
    {
        VC_PushLogMessage("Couldn't open input file", VC_LOG_ERROR);
        return -1;
    }

    if (options.sOutFilePath[0] == '\0')
    {
        char sInFilePathCopy[VC_PATH_LEN];
        strncpy(sInFilePathCopy, options.sInFilePath, VC_PATH_LEN);
        strncat(sInFilePathCopy, ".ogg", VC_PATH_LEN);
        VC_OpenOutputFile(sInFilePathCopy);
    }
    else
    {
        VC_OpenOutputFile(options.sOutFilePath);
    }
    
    VC_ReadHeader();

    if (!vc_ctx.bValid)
    {
        VC_PushLogMessage("Couldn't open output file", VC_LOG_ERROR);
        return -1;
    }

    int status;

    srand(time(NULL));
    status = ogg_stream_init(&vc_ctx.stream, rand());
    if (status < 0)
    {
        VC_PushLogMessage("Couldn't initialize vorbis stream", VC_LOG_ERROR);
        return -1;
    }

    vorbis_info_init(&vc_ctx.vi);
    status = vorbis_encode_init_vbr(&vc_ctx.vi, vc_ctx.common.nChannels, vc_ctx.common.nSamplesPerSec, options.desired_quality);
    if (status < 0)
    {
        VC_PushLogMessage("Couldn't initialize vorbis encoding engine", VC_LOG_ERROR);
        return -1;
    }

    status = vorbis_analysis_init(&vc_ctx.dsp, &vc_ctx.vi);
    if (status < 0)
    {
        VC_PushLogMessage("Couldn't initialize vorbis analysis engine", VC_LOG_ERROR);
        return -1;
    }
    
    vorbis_comment_init(&vc_ctx.comment);
    ogg_packet header_packet, comment_packet, code_packet;
    status = vorbis_analysis_headerout(&vc_ctx.dsp, &vc_ctx.comment, &header_packet, &comment_packet, &code_packet);
    if (status < 0)
    {
        VC_PushLogMessage("Failed to write header", VC_LOG_ERROR);
        return -1;
    }

    status = vorbis_block_init(&vc_ctx.dsp, &vc_ctx.block);
    if (status < 0)
    {
        VC_PushLogMessage("Couldn't initialize vorbis block structure", VC_LOG_ERROR);
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
        fwrite(vc_ctx.page.header, 1, vc_ctx.page.header_len, vc_ctx.pOutfile);
        fwrite(vc_ctx.page.body, 1, vc_ctx.page.body_len, vc_ctx.pOutfile);
    }
    
    bool eos = false;

    while (!eos)
    {
        size_t n_bytes = fread(vc_read_buffer, 1, VC_RB_SIZE, vc_ctx.pInfile);
        if ((status = ferror(vc_ctx.pInfile)))
        {
            char message[VC_LOG_MSG_MAXLEN] = { 0 };
            snprintf(message, VC_LOG_MSG_MAXLEN, "File error: %d", status);
            VC_PushLogMessage(message, VC_LOG_ERROR);
            return -1;
        }

        if (n_bytes == 0 || feof(vc_ctx.pInfile))
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
                char message[VC_LOG_MSG_MAXLEN] = { 0 };
                snprintf(message, VC_LOG_MSG_MAXLEN, "Analysis error: %d", status);
                VC_PushLogMessage(message, VC_LOG_ERROR);
                return -1;
            }
            status = vorbis_bitrate_addblock(&vc_ctx.block);
            if (status < 0)
            {
                char message[VC_LOG_MSG_MAXLEN] = { 0 };
                snprintf(message, VC_LOG_MSG_MAXLEN, "Couldn't add block: %d", status);
                VC_PushLogMessage(message, VC_LOG_ERROR);
                return -1;
            }

            while ((status = vorbis_bitrate_flushpacket(&vc_ctx.dsp, &vc_ctx.packet)) > 0)
            {
                status = ogg_stream_packetin(&vc_ctx.stream, &vc_ctx.packet);
                if (status < 0)
                {
                    char message[VC_LOG_MSG_MAXLEN] = { 0 };
                    snprintf(message, VC_LOG_MSG_MAXLEN, "Failed to read packet from stream: %d", status);
                    VC_PushLogMessage(message, VC_LOG_ERROR);
                    return -1;
                }
                while (!eos)
                {
                    int result = ogg_stream_pageout(&vc_ctx.stream, &vc_ctx.page);
                    if (result == 0)
                    {
                        break;
                    }

                    fwrite(vc_ctx.page.header, 1, vc_ctx.page.header_len, vc_ctx.pOutfile);
                    fwrite(vc_ctx.page.body, 1, vc_ctx.page.body_len, vc_ctx.pOutfile);
                    if(ogg_page_eos(&vc_ctx.page))
                    {
                        eos = true;
                    }
                }
            }
            if (status < 0)
            {
                char message[VC_LOG_MSG_MAXLEN] = { 0 };
                snprintf(message, VC_LOG_MSG_MAXLEN, "Failed to flush packet: %d", status);
                VC_PushLogMessage(message, VC_LOG_ERROR);
                return -1;
            }

        }
        if (status < 0)
        {
            char message[VC_LOG_MSG_MAXLEN] = { 0 };
            snprintf(message, VC_LOG_MSG_MAXLEN, "Couldn't write block: %d", status);
            VC_PushLogMessage(message, VC_LOG_ERROR);
            return -1;
        }
        
    }

    ogg_stream_clear(&vc_ctx.stream);
    vorbis_block_clear(&vc_ctx.block);
    vorbis_dsp_clear(&vc_ctx.dsp);
    vorbis_comment_clear(&vc_ctx.comment);
    vorbis_info_clear(&vc_ctx.vi);
    

    return 0;
}

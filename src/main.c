#include "encoding/encoding.h"
#include "logging/logging.h"

int main(int argc, char const *argv[])
{
    int status;

    status = VC_Encode((VC_EncodeOptions)
    {
        .desired_quality = 0.1,
        .sInFilePath = "../M1F1-int16WE-AFsp.wav",
        .sOutFilePath = "../M1F1-int16WE-AFsp.ogg",
    });

    if (status < 0)
    {
        VC_ReadLogQueue();
        exit(1);
    }
    
    VC_PushLogMessage("File encoded successfully.", VC_LOG_INFO);
    VC_ReadLogQueue();
    
    return 0;
}

#ifndef VC_ENCODING_H
#define VC_ENCODING_H

#include <stdint.h>
#include <stdbool.h>

#define VC_RB_SIZE 1024
#define VC_PATH_LEN 260

typedef struct 
{
    char sInFilePath[VC_PATH_LEN];
    char sOutFilePath[VC_PATH_LEN];
    float desired_quality;

} VC_EncodeOptions;

bool VC_IsStateValid();
int VC_Encode(VC_EncodeOptions options);

#endif //VC_ENCODING_H
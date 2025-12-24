#ifndef VC_ENCODING_H
#define VC_ENCODING_H

#include <stdint.h>
#include <stdbool.h>

#include "options.h"

#define VC_RB_SIZE 3072

int VC_Encode(VC_EncodeOptions options);

#endif //VC_ENCODING_H
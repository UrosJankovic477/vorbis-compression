#ifndef VC_ENCODING_H
#define VC_ENCODING_H

#include <stdint.h>
#include <stdbool.h>
#include "options.h"

#define VC_BUFFER_SIZE 3072

GThread *VcEncode(VcEncodeOptions *options);
GThread *VcGetEncoderThread();

#endif //VC_ENCODING_H
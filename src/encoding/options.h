#ifndef VC_OPTIONS_H
#define VC_OPTIONS_H

#include <gtk-4.0/gtk/gtk.h>

#define VC_PATH_LEN 260

typedef struct 
{
    GFileInputStream *pInFileStream;
    GFileOutputStream *pOutFileStream;
    float fDesiredQuality;

} VC_EncodeOptions;

#endif // VC_OPTIONS_H
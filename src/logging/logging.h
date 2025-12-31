#ifndef VC_LOGGING_H
#define VC_LOGGING_H

#include <stdlib.h>

#define VC_LOG_MSG_MAXLEN 255

typedef enum
{
    VC_LOG_INFO,
    VC_LOG_WARNING,
    VC_LOG_ERROR,
    VC_LOG_NONE = 255

} VcLogMessagePriority;

void VcPushLogMessage(const char *message, VcLogMessagePriority priority);
void VcReadLogQueue();

typedef void (*VC_LoggerCallback)(const char *message, VcLogMessagePriority priority);

void VcWriteToLogFile(const char *message, VcLogMessagePriority priority);
void VcWriteToStderr(const char *message, VcLogMessagePriority priority);

void VcSetLoggerCallback(VC_LoggerCallback cb);

void VcSetLogPriority(VcLogMessagePriority priority);

#endif // VC_LOGGING_H
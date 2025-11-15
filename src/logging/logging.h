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

} VC_LogMessagePriority;

void VC_PushLogMessage(const char *message, VC_LogMessagePriority priority);
void VC_ReadLogQueue();

typedef void (*VC_LoggerCallback)(const char *message, VC_LogMessagePriority priority);

void VC_WriteToLogFile(const char *message, VC_LogMessagePriority priority);
void VC_WriteToStderr(const char *message, VC_LogMessagePriority priority);

void VC_SetLoggerCallback(VC_LoggerCallback cb);

void VC_SetLogPriority(VC_LogMessagePriority priority);

#endif // VC_LOGGING_H
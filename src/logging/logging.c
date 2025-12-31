#include "logging.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define VC_LOG_MSG_QUEUE_LEN 64
#define VC_LOG_FILEPATH "../log.txt"

typedef struct VC_LogMessage
{
    char sMessage[VC_LOG_MSG_MAXLEN];
    uint8_t ePriority;

} VC_LogMessage;

static VC_LogMessage aMessageQueue[VC_LOG_MSG_QUEUE_LEN];
static size_t nMessageCount = 0;
static VcLogMessagePriority eMinPriority = VC_LOG_INFO;

void VC_WriteToLogStream(const char *message, VcLogMessagePriority priority, FILE *stream)
{
    const char *prefix;
    size_t prefixLen;
    switch (priority)
    {
        case VC_LOG_INFO:
        {
            prefix = "INFO";
            break;
        }

        case VC_LOG_WARNING:
        {
            prefix = "WARNING";
            break;
        }

        case VC_LOG_ERROR:
        {
            prefix = "ERROR";
            break;
        }

        default:
        {
            prefix = "";
            prefixLen = 0;
            break;
        }
    }

    fprintf(stream, "[%s] %s\n", prefix, message);
}

void VcWriteToLogFile(const char *message, VcLogMessagePriority priority)
{
    FILE *logfile = fopen(VC_LOG_FILEPATH, "a");
    VC_WriteToLogStream(message, priority, logfile);
    fclose(logfile);
}

void VcWriteToStderr(const char *message, VcLogMessagePriority priority)
{
    VC_WriteToLogStream(message, priority, stderr);
}

VC_LoggerCallback loggerCallback = VcWriteToStderr;

void VcPushLogMessage(const char *message, VcLogMessagePriority priority)
{
    if (priority < eMinPriority)
    {
        return;
    }

    if (nMessageCount == VC_LOG_MSG_QUEUE_LEN)
    {
        VC_LoggerCallback oldCb = loggerCallback;
        loggerCallback = VcWriteToLogFile;
        VcReadLogQueue();
        loggerCallback = oldCb;
    }
    
    aMessageQueue[nMessageCount].ePriority = priority;
    strncpy(aMessageQueue[nMessageCount].sMessage, message, VC_LOG_MSG_MAXLEN);
    nMessageCount++;
}

void VcReadLogQueue()
{
    for (size_t i = 0; i < nMessageCount; i++)
    {
        if ((VcLogMessagePriority) aMessageQueue[i].ePriority < eMinPriority)
        {
            continue;
        }
        
        loggerCallback(aMessageQueue[i].sMessage, (VcLogMessagePriority) aMessageQueue[i].ePriority);
    }

    nMessageCount = 0;
}

void VcSetLoggerCallback(VC_LoggerCallback cb)
{
    loggerCallback = cb;
}

void VcSetLogPriority(VcLogMessagePriority priority) 
{
    eMinPriority = priority;
}

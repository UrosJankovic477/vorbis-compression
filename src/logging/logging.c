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
static VC_LogMessagePriority eMinPriority = VC_LOG_INFO;

void VC_WriteToLogStream(const char *message, VC_LogMessagePriority priority, FILE *stream)
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

void VC_WriteToLogFile(const char *message, VC_LogMessagePriority priority)
{
    FILE *logfile = fopen(VC_LOG_FILEPATH, "a");
    VC_WriteToLogStream(message, priority, logfile);
    fclose(logfile);
}

void VC_WriteToStderr(const char *message, VC_LogMessagePriority priority)
{
    VC_WriteToLogStream(message, priority, stderr);
}

VC_LoggerCallback loggerCallback = VC_WriteToStderr;

void VC_PushLogMessage(const char *message, VC_LogMessagePriority priority)
{
    if (priority < eMinPriority)
    {
        return;
    }

    if (nMessageCount == VC_LOG_MSG_QUEUE_LEN)
    {
        VC_LoggerCallback oldCb = loggerCallback;
        loggerCallback = VC_WriteToLogFile;
        VC_ReadLogQueue();
        loggerCallback = oldCb;
    }
    
    aMessageQueue[nMessageCount].ePriority = priority;
    strncpy(aMessageQueue[nMessageCount].sMessage, message, VC_LOG_MSG_MAXLEN);
    nMessageCount++;
}

void VC_ReadLogQueue()
{
    for (size_t i = 0; i < nMessageCount; i++)
    {
        if ((VC_LogMessagePriority) aMessageQueue[i].ePriority < eMinPriority)
        {
            continue;
        }
        
        loggerCallback(aMessageQueue[i].sMessage, (VC_LogMessagePriority) aMessageQueue[i].ePriority);
    }

    nMessageCount = 0;
}

void VC_SetLoggerCallback(VC_LoggerCallback cb)
{
    loggerCallback = cb;
}

void VC_SetLogPriority(VC_LogMessagePriority priority) 
{
    eMinPriority = priority;
}

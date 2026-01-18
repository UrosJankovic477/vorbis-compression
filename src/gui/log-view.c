#include "log-view.h"
#include <string.h>

#define VC_LINE_SIZE 1028

gchar charBuffer[VC_LINE_SIZE];

void VcLogViewClear(GtkTextView *_logView)
{
    GtkTextIter iterStart, iterEnd;
    GtkTextBuffer *_textBuffer = gtk_text_view_get_buffer(_logView);
    gtk_text_buffer_get_start_iter(_textBuffer, &iterStart);
    gtk_text_buffer_get_end_iter(_textBuffer, &iterEnd);
    gtk_text_buffer_delete(_textBuffer, &iterStart, &iterEnd);
}

void VcLogViewWriteLine(GtkTextView *_logView, const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    GtkTextBuffer *_textBuffer = gtk_text_view_get_buffer(_logView);
    int len = vsnprintf(charBuffer, VC_LINE_SIZE - 1, format, args);
    va_end(args);
    gtk_text_buffer_insert_at_cursor(_textBuffer, g_strconcat(charBuffer, "\n"), len + 1);
}

void VcLogViewCopy(GtkTextView *_logView)
{
    GtkTextBuffer *_textBuffer = gtk_text_view_get_buffer(_logView);
    GdkDisplay *display = gdk_display_get_default();
    GdkClipboard *clipboard = gdk_display_get_clipboard(display);
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(_textBuffer, &start);
    gtk_text_buffer_get_end_iter(_textBuffer, &end);
    gdk_clipboard_set_text(clipboard, gtk_text_buffer_get_text(_textBuffer, &start, &end, true));
}
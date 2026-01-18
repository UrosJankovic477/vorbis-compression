#ifndef VC_LOG_VIEW_H
#define VC_LOG_VIEW_H

#include <gtk-4.0/gtk/gtk.h>
#include <stdarg.h>

void VcLogViewClear(GtkTextView *_logView);
void VcLogViewWriteLine(GtkTextView *_logView, const char *format, ...);
void VcLogViewCopy(GtkTextView *_logView);

#endif // VC_LOG_VIEW_H
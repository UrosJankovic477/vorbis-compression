#include "gui.h"
#include "log-view.h"
#include "../encoding/options.h"
#include "../encoding/encoding.h"
#include "../audio-io/audio-io.h"

static VcEncodeOptions  encodingOptions      = { 0 };
static GtkWidget        *inputFileLabel         = NULL;
static GtkWidget        *outputFileLabel        = NULL;
static GtkWidget        *compressionRateLabel   = NULL;
static size_t           inputFileSize           = 0;
static size_t           outputFileSize          = 0;
static GtkWidget        *playbackButton         = NULL; 
static GtkWidget        *stopButton             = NULL;
static GtkWidget        *logView                = NULL;
static GtkWidget        *chooseFileButton       = NULL;
static GtkWidget        *convertButton          = NULL;
static GtkTextBuffer    *textBuffer             = NULL;
static GtkWidget        *spinner                = NULL;
static GTimer           *timer                  = NULL;
static GThread          *encoderThread          = NULL;
static GFile            *outFile                = NULL;

void VcOnEncodeFinished(gpointer data)
{
    VcEncodeOptions *_encodingOptions = (VcEncodeOptions *)data;
    int status = (int)g_thread_join(encoderThread);
    g_timer_stop(timer);
    if (status < 0)
    {
        VcLogViewWriteLine(GTK_TEXT_VIEW(logView), "Encription failed!");
        gtk_spinner_stop(GTK_SPINNER(spinner));
        gtk_widget_set_sensitive(convertButton, true);
        gtk_widget_set_sensitive(chooseFileButton, true);
        return;
    }

    GFileOutputStream *outFileStream = _encodingOptions->pOutFileStream;

    gulong microseconds = 0;
    gdouble seconds = g_timer_elapsed(timer, &microseconds);

    VcLogViewWriteLine(GTK_TEXT_VIEW(logView), "Encription done! Elapsed time: %000.lf.%06ds", seconds, microseconds);

    GError *error = NULL;

    GFileInfo *outFileInfo = g_file_output_stream_query_info(outFileStream, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, &error);
    if (error != NULL)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        g_error_free(error);
        return;
    }

    const char *outFilePath = g_file_get_path(G_FILE(outFile));

    if (VcAudioIoIsInitialized())
    {
        VcAudioIoFinalize();
    }
    VcAudioIoInit(outFilePath);

    VcToggleMediaControls(true);

    gtk_button_set_icon_name(playbackButton, "media-playback-start");
    
    outputFileSize = g_file_info_get_size(outFileInfo);
    
    gtk_label_set_label(GTK_LABEL(outputFileLabel), outFilePath);
    if (inputFileSize != 0)
    {
        VcLogViewWriteLine(GTK_TEXT_VIEW(logView), "Input size: %d bytes", inputFileSize);
        VcLogViewWriteLine(GTK_TEXT_VIEW(logView), "Output size: %d bytes", outputFileSize);
        VcLogViewWriteLine(GTK_TEXT_VIEW(logView), "Compression rate: %3.2f%%", (float)(outputFileSize) / (float)(inputFileSize) * 100);
        VcLogViewWriteLine(GTK_TEXT_VIEW(logView), "Saved at %s", outFilePath);
        gtk_widget_set_visible(GTK_WIDGET(outputFileLabel), true);
    }

    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "File encoded successfully.");

    gtk_spinner_stop(GTK_SPINNER(spinner));

    gtk_widget_set_sensitive(convertButton, true);
    gtk_widget_set_sensitive(chooseFileButton, true);

    g_output_stream_close(outFileStream, NULL, NULL);

    free(outFilePath);
}

void VcToggleMediaControls(gboolean state)
{
    gtk_widget_set_sensitive(playbackButton, state);
    gtk_widget_set_sensitive(stopButton, state);
}

void VcFileReadFinished(GObject *inFile, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFileInputStream *inFileStream = g_file_read_finish(G_FILE(inFile), res, &error);
    if (error != NULL)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        g_error_free(error);
        return;
    }
    
    const char *inFilePath = g_file_get_path(G_FILE(inFile));
    gtk_label_set_label(GTK_LABEL(inputFileLabel), inFilePath);
    GFileInfo *inFileInfo = g_file_input_stream_query_info(inFileStream, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, &error);
    if (error != NULL)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        g_error_free(error);
        return;
    }

    inputFileSize = g_file_info_get_size(inFileInfo);
    
    encodingOptions.pInFileStream = inFileStream;
}

void VcOnInputFileDialogFinished(GObject *fileDialog, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFile *inFile = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(fileDialog), res, &error);
    if (error != NULL)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        VcLogViewWriteLine(GTK_TEXT_VIEW(logView), error->message);
        g_error_free(error);
        return;
    }

    const char *path = g_file_get_path(G_FILE(inFile));
    VcLogViewWriteLine(GTK_TEXT_VIEW(logView), "Selected file at %s", path);
    free(path);

    gtk_widget_set_sensitive(convertButton, true);

    g_file_read_async(inFile, G_PRIORITY_DEFAULT, NULL, VcFileReadFinished, NULL);
}

void VcOnOpenFileClicked(GtkFileDialog *fileDialog)
{
    gtk_file_dialog_open(fileDialog, NULL, NULL, VcOnInputFileDialogFinished, NULL);
}

void VcOnOutputFileDialogFinished(GObject *fileDialog, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    if (outFile != NULL)
    {
        g_free(outFile);
    }
    
    outFile = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(fileDialog), res, &error);
    if (error != NULL)
    {
        gtk_spinner_stop(GTK_SPINNER(spinner));

        gtk_widget_set_sensitive(convertButton, true);
        gtk_widget_set_sensitive(chooseFileButton, true);
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        g_error_free(error);
        return;
    }

    const char *outFilePath = g_file_get_path(G_FILE(outFile));

    GFileOutputStream *outFileStream = g_file_replace(outFile, NULL, false, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);
    if (error != NULL)
    {
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, error->message);
        g_error_free(error);
        return;
    }

    encodingOptions.pOutFileStream  = outFileStream;
    encodingOptions.pLogView        = logView;
    encodingOptions.cbOnFinished    = VcOnEncodeFinished;
    g_timer_start(timer);
    encoderThread                   = VcEncode(&encodingOptions);
    
    free(outFilePath);
}

void VcOnConvertClicked(GtkFileDialog *outputFileDialog)
{
    gtk_spinner_start(GTK_SPINNER(spinner));
    gtk_widget_set_sensitive(convertButton, false);
    gtk_widget_set_sensitive(chooseFileButton, false);
    gtk_file_dialog_save(outputFileDialog, NULL, NULL, VcOnOutputFileDialogFinished, NULL);
}

void VcOnPlaybackButtonClick(GObject *button)
{
    if (!VcAudioIoIsInitialized())
    {
        return;
    }

    bool paused = VcAudioIoTogglePlayback();

    if (paused)
    {
        gtk_button_set_icon_name(GTK_BUTTON(button), "media-playback-pause");
    }

    else
    {
        gtk_button_set_icon_name(GTK_BUTTON(button), "media-playback-start");
    }

}

void VcOnStopButtonClick(GObject *button)
{
    if (!VcAudioIoIsInitialized())
    {
        return;
    }

    gtk_button_set_icon_name(GTK_BUTTON(playbackButton), "media-playback-start");

    VcAudioIoReset();
}

void VcOnActivate(GtkApplication *app)
{
    GtkWidget       *window             = gtk_application_window_new(app);
    
    GtkFileFilter   *inFileFilter       = gtk_file_filter_new();
    GtkFileFilter   *outFileFilter      = gtk_file_filter_new();
    GtkFileDialog   *inputFileDialog    = gtk_file_dialog_new();
    GtkFileDialog   *outputFileDialog   = gtk_file_dialog_new();

    gtk_window_set_default_size(GTK_WINDOW(window), 600, 420);
    gtk_file_filter_add_mime_type(inFileFilter, "audio/wav");
    gtk_file_filter_add_mime_type(outFileFilter, "audio/ogg");
    gtk_file_dialog_set_default_filter(inputFileDialog, inFileFilter);
    gtk_file_dialog_set_default_filter(outputFileDialog, outFileFilter);

    inputFileLabel          = gtk_label_new("(no file selected)");
    outputFileLabel         = gtk_label_new("");
    compressionRateLabel    = gtk_label_new("");
    chooseFileButton        = gtk_button_new_with_label("Choose File");
    convertButton           = gtk_button_new_with_label("Convert");

    gtk_widget_set_sensitive(convertButton, false);

    gtk_widget_set_halign(inputFileLabel, GTK_ALIGN_START);
    gtk_widget_set_halign(outputFileLabel, GTK_ALIGN_START);

    gtk_widget_set_visible(GTK_WIDGET(outputFileLabel), false);

    playbackButton  = gtk_button_new_from_icon_name("media-playback-start");
    stopButton      = gtk_button_new_from_icon_name("media-playback-stop");
    textBuffer      = gtk_text_buffer_new(NULL);
    logView         = gtk_text_view_new_with_buffer(textBuffer);
    spinner         = gtk_spinner_new();

    VcToggleMediaControls(false);
 
    GtkWidget   *mainBox        = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget   *controlsBox    = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget   *grid           = gtk_grid_new();
    GtkWidget   *scrolledWindow = gtk_scrolled_window_new();
    GtkWidget   *clearLogButton = gtk_button_new_with_label("Clear Log");
    GtkWidget   *copyLogButton  = gtk_button_new_from_icon_name("edit-copy");

    gtk_text_view_set_editable(logView, false);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledWindow), logView);
    gtk_widget_set_hexpand(copyLogButton, false);
    gtk_widget_set_halign(copyLogButton, GTK_ALIGN_START);
    gtk_widget_set_margin_top(copyLogButton, 20);
    gtk_widget_set_margin_bottom(copyLogButton, 0);
    gtk_widget_set_margin_start(copyLogButton, 10);
    gtk_widget_set_tooltip_text(copyLogButton, "Copy log to the clipboard");

    gtk_widget_add_css_class(clearLogButton, "destructive-action");
    gtk_widget_add_css_class(convertButton, "suggested-action");

    gtk_grid_set_column_homogeneous(GTK_GRID(grid), true);
    gtk_box_append(GTK_BOX(mainBox), grid);
    gtk_box_append(GTK_BOX(mainBox), controlsBox);

    gtk_widget_set_margin_top(grid, 10);
    gtk_widget_set_margin_start(grid, 10);
    gtk_widget_set_margin_end(grid, 10);

    gtk_widget_set_margin_start(controlsBox, 10);
    gtk_widget_set_margin_end(controlsBox, 10);

    gtk_widget_set_margin_top(clearLogButton, 2);
    gtk_widget_set_margin_bottom(clearLogButton, 10);
    gtk_widget_set_margin_start(clearLogButton, 10);
    gtk_widget_set_margin_end(clearLogButton, 10);

    gtk_box_append(GTK_BOX(controlsBox), playbackButton);
    gtk_box_append(GTK_BOX(controlsBox), stopButton);
    gtk_box_append(GTK_BOX(mainBox), copyLogButton);
    gtk_box_append(GTK_BOX(mainBox), scrolledWindow);
    gtk_box_append(GTK_BOX(mainBox), clearLogButton);

    gtk_widget_set_margin_top(logView, 10);
    gtk_widget_set_margin_bottom(logView, 0);
    gtk_widget_set_margin_start(logView, 10);
    gtk_widget_set_margin_end(logView, 10);
    
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

    gtk_widget_set_vexpand(logView, true);
    gtk_widget_set_margin_bottom(logView, 10);

    gtk_window_set_child(GTK_WINDOW(window), mainBox);

    gtk_grid_attach(GTK_GRID(grid), chooseFileButton, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), inputFileLabel, 2, 0, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), convertButton, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spinner, 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), outputFileLabel, 2, 2, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), compressionRateLabel, 1, 3, 1, 1);
    
    g_signal_connect_swapped(clearLogButton, "clicked", G_CALLBACK(VcLogViewClear), logView);
    g_signal_connect_swapped(chooseFileButton, "clicked", G_CALLBACK(VcOnOpenFileClicked), inputFileDialog);
    g_signal_connect_swapped(convertButton, "clicked", G_CALLBACK(VcOnConvertClicked), outputFileDialog);
    g_signal_connect_swapped(playbackButton, "clicked", G_CALLBACK(VcOnPlaybackButtonClick), playbackButton);
    g_signal_connect_swapped(stopButton, "clicked", G_CALLBACK(VcOnStopButtonClick), stopButton);
    g_signal_connect_swapped(copyLogButton, "clicked", G_CALLBACK(VcLogViewCopy), logView);

    gtk_window_set_icon_name(GTK_WINDOW(window), "applications-multimedia");
    
    gtk_window_present(GTK_WINDOW(window));
}

int VcRunApp(int argc, char** argv)
{
    timer = g_timer_new();

    GtkApplication *app = gtk_application_new("vc.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(VcOnActivate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    VcAudioIoFinalize();

    g_timer_destroy(timer);

    return status;
}


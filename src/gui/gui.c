#include "gui.h"
#include "../logging/logging.h"
#include "../encoding/options.h"
#include "../encoding/encoding.h"
#include "../audio-io/audio-io.h"

static VcEncodeOptions  vc_encodingOptions      = { 0 };
static GtkWidget        *inputFileLabel         = NULL;
static GtkWidget        *outputFileLabel        = NULL;
static GtkWidget        *compressionRateLabel   = NULL;
static size_t           inputFileSize           = 0;
static size_t           outputFileSize          = 0;
static double           timestamp               = 0.0;
static double           duration                = 0.0;

void VcFileReadFinished(GObject *inFile, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFileInputStream *inFileStream = g_file_read_finish(G_FILE(inFile), res, &error);
    if (error != NULL)
    {
        VcPushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }
    
    const char *inFilePath = g_file_get_path(G_FILE(inFile));
    gtk_label_set_label(GTK_LABEL(inputFileLabel), inFilePath);
    GFileInfo *inFileInfo = g_file_input_stream_query_info(inFileStream, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, &error);
    if (error != NULL)
    {
        VcPushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    inputFileSize = g_file_info_get_size(inFileInfo);
    
    vc_encodingOptions.pInFileStream = inFileStream;
}

void VcOnInputFileDialogFinished(GObject *fileDialog, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFile *inFile = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(fileDialog), res, &error);
    if (error != NULL)
    {
        VcPushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    g_file_read_async(inFile, G_PRIORITY_DEFAULT, NULL, VcFileReadFinished, NULL);
}

void VcOnOpenFileClicked(GtkFileDialog *fileDialog)
{
    gtk_file_dialog_open(fileDialog, NULL, NULL, VcOnInputFileDialogFinished, NULL);
    VcReadLogQueue();
}

void VcOnOutputFileDialogFinished(GObject *fileDialog, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFile *outFile = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(fileDialog), res, &error);
    if (error != NULL)
    {
        VcPushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    const char *outFilePath = g_file_get_path(G_FILE(outFile));

    GFileOutputStream *outFileStream = g_file_replace(outFile, NULL, false, G_FILE_CREATE_NONE, NULL, &error);
    if (error != NULL)
    {
        VcPushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    vc_encodingOptions.pOutFileStream = outFileStream;
    int status = VcEncode(vc_encodingOptions);
    if (status < 0)
    {
        VcReadLogQueue();
        return;
    }

    GFileInfo *outFileInfo = g_file_output_stream_query_info(outFileStream, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, &error);
    if (error != NULL)
    {
        VcPushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    if (!VcAudioIoIsInitialized())
    {
        VcAudioIoFinalize();
    }
    VcAudioIoInit(outFilePath);
    duration = VcAudioIoGetDuration();
    
    outputFileSize = g_file_info_get_size(outFileInfo);
    
    gtk_label_set_label(GTK_LABEL(outputFileLabel), outFilePath);
    if (inputFileSize != 0)
    {
        char compressionRatioString[26] = { 0 };
        snprintf(compressionRatioString, 26, "Compression rate: %3.2f%%", (float)(outputFileSize) / (float)(inputFileSize) * 100);
        gtk_label_set_label(GTK_LABEL(compressionRateLabel), compressionRatioString);
        gtk_widget_set_visible(GTK_WIDGET(outputFileLabel), true);
    }
    
    VcPushLogMessage("File encoded successfully.", VC_LOG_INFO);
    VcReadLogQueue();
    
}

void VcOnConvertClicked(GtkFileDialog *outputFileDialog)
{
    gtk_file_dialog_save(outputFileDialog, NULL, NULL, VcOnOutputFileDialogFinished, NULL);
    VcReadLogQueue();
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

    VcAudioIoSeek(0);
}

void VcOnChangeVolume(GObject *slider, GAsyncResult *res, gpointer data)
{
    double volume = gtk_scale_button_get_value(GTK_SCALE_BUTTON(slider));
    VcAudioIoSetVolume(volume);
}

void VcOnSeek(GObject *slider, GAsyncResult *res, gpointer data)
{
    if (!VcAudioIoIsInitialized())
    {
        return;
    }

    double timestamp = gtk_range_get_value(GTK_RANGE(slider));
    VcAudioIoSeek(timestamp);
}

void VcOnActivate(GtkApplication *app)
{
    GtkWidget       *window             = gtk_application_window_new(app);
    GtkWidget       *chooseFileButton   = gtk_button_new_with_label("Choose File");
    GtkWidget       *convertButton      = gtk_button_new_with_label("Convert");
    GtkFileFilter   *fileFilter         = gtk_file_filter_new();
    GtkFileDialog   *inputFileDialog    = gtk_file_dialog_new();
    GtkFileDialog   *outputFileDialog   = gtk_file_dialog_new();

    gtk_window_set_default_size(GTK_WINDOW(window), 600, 420);
    gtk_file_filter_add_mime_type(fileFilter, "audio/wav");
    gtk_file_dialog_set_default_filter(inputFileDialog, fileFilter);

    inputFileLabel          = gtk_label_new("(no file selected)");
    outputFileLabel         = gtk_label_new("");
    compressionRateLabel    = gtk_label_new("");

    gtk_widget_set_visible(GTK_WIDGET(outputFileLabel), false);

    const char *icons[] = 
    {
        "audio-volume-muted",
        "audio-volume-high",
        "audio-volume-low",
        "audio-volume-medium",
        NULL
    };

    GtkWidget *playbackButton   = gtk_button_new_from_icon_name("media-playback-pause");
    GtkWidget *stopButton       = gtk_button_new_from_icon_name("media-playback-stop");
    GtkWidget *volumeSlider     = gtk_scale_button_new
    (
        0.0, 1.0, 0.1, 
        (const char **)icons
    );

    gtk_scale_button_set_value(volumeSlider, 1.0);

    GtkWidget   *streamSlider   = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
    GtkWidget   *mainBox        = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget   *controlsBox    = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget   *grid           = gtk_grid_new();

    gtk_box_append(GTK_BOX(mainBox), grid);
    gtk_box_append(GTK_BOX(mainBox), controlsBox);
    gtk_box_append(GTK_BOX(controlsBox), playbackButton);
    gtk_box_append(GTK_BOX(controlsBox), stopButton);
    gtk_box_append(GTK_BOX(controlsBox), volumeSlider);
    gtk_box_append(GTK_BOX(controlsBox), streamSlider);

    gtk_widget_set_hexpand(streamSlider, true);
    gtk_widget_set_margin_end(streamSlider, 60);

    gtk_window_set_child(GTK_WINDOW(window), mainBox);

    gtk_grid_attach(GTK_GRID(grid), chooseFileButton, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), convertButton, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), inputFileLabel, 0, 1, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), outputFileLabel, 0, 3, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), compressionRateLabel, 0, 4, 1, 1);
    
    g_signal_connect_swapped(chooseFileButton, "clicked", G_CALLBACK(VcOnOpenFileClicked), inputFileDialog);
    g_signal_connect_swapped(convertButton, "clicked", G_CALLBACK(VcOnConvertClicked), outputFileDialog);
    g_signal_connect_swapped(playbackButton, "clicked", G_CALLBACK(VcOnPlaybackButtonClick), playbackButton);
    g_signal_connect_swapped(stopButton, "clicked", G_CALLBACK(VcOnStopButtonClick), stopButton);
    g_signal_connect_swapped(volumeSlider, "value-changed", G_CALLBACK(VcOnChangeVolume), volumeSlider);
    g_signal_connect_swapped(streamSlider, "value-changed", G_CALLBACK(VcOnSeek), streamSlider);
    
    gtk_window_present(GTK_WINDOW(window));
}

int VcRunApp(int argc, char** argv)
{

    GtkApplication *app = gtk_application_new("vc.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(VcOnActivate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    VcAudioIoFinalize();

    return status;
}


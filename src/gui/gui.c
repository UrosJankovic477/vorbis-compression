#include "gui.h"
#include "../logging/logging.h"
#include "../encoding/options.h"
#include "../encoding/encoding.h"

static VcEncodeOptions     vc_encodingOptions      = { 0 };
static GtkWidget            *inputFileLabel         = NULL;
static GtkWidget            *outputFileLabel        = NULL;
static GtkWidget            *compressionRateLabel   = NULL;
static GtkMediaStream       *mediaStream            = NULL;
static GtkMediaControls     *mediaControlls         = NULL;
static size_t               inputFileSize           = 0;
static size_t               outputFileSize          = 0;

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

void VcOnChooseFileClicked(GtkFileDialog *fileDialog)
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

    GFileOutputStream *outFileStream = g_file_replace(outFile, NULL, false, G_FILE_CREATE_NONE, NULL, &error);
    if (error != NULL)
    {
        VcPushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    mediaStream = gtk_media_file_new_for_file(outFile);
    gtk_media_controls_set_media_stream(mediaControlls, mediaStream);

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
    
    outputFileSize = g_file_info_get_size(outFileInfo);

    const char *outFilePath = g_file_get_path(G_FILE(outFile));
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

void VcOnActivate(GtkApplication *app)
{
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 420);

    GtkWidget *chooseFileButton = gtk_button_new_with_label("Choose File");
    GtkWidget *convertButton = gtk_button_new_with_label("Convert");

    GtkFileFilter *fileFilter = gtk_file_filter_new();
    gtk_file_filter_add_mime_type(fileFilter, "audio/*");

    GtkFileDialog *inputFileDialog = gtk_file_dialog_new();
    GtkFileDialog *outputFileDialog = gtk_file_dialog_new();

    gtk_file_dialog_set_default_filter(inputFileDialog, fileFilter);

    inputFileLabel = gtk_label_new("(no file selected)");
    outputFileLabel = gtk_label_new("");
    compressionRateLabel = gtk_label_new("");

    mediaControlls = gtk_media_file_new();
    mediaControlls = gtk_media_controls_new(NULL);

    gtk_widget_set_visible(GTK_WIDGET(outputFileLabel), false);

    GtkWidget *grid = gtk_grid_new();
    gtk_window_set_child(GTK_WINDOW(window), grid);
    gtk_grid_attach(GTK_GRID(grid), chooseFileButton, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), convertButton, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), inputFileLabel, 0, 1, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), outputFileLabel, 0, 3, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), compressionRateLabel, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), mediaControlls, 0, 5, 1, 1);
    g_signal_connect_swapped(chooseFileButton, "clicked", G_CALLBACK(VcOnChooseFileClicked), inputFileDialog);
    g_signal_connect_swapped(convertButton, "clicked", G_CALLBACK(VcOnConvertClicked), outputFileDialog);
    gtk_window_present(GTK_WINDOW(window));
}

int VcRunApp(int argc, char** argv)
{
    GtkTextTagTable *textTagTable = gtk_text_tag_table_new();

    GtkApplication *app = gtk_application_new("vc.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(VcOnActivate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);

    return status;
}


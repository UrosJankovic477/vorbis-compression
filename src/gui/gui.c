#include "gui.h"
#include "../logging/logging.h"
#include "../encoding/options.h"
#include "../encoding/encoding.h"

static VC_EncodeOptions vc_encodingOptions = { 0 };
static GtkTextBuffer *inputFileTextBuffer = NULL;
static GtkTextBuffer *outputFileTextBuffer = NULL;

void VC_FileReadFinished(GObject *inFile, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFileInputStream *inFileStream = g_file_read_finish(G_FILE(inFile), res, &error);
    if (error != NULL)
    {
        VC_PushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }
    
    const char *inFilePath = g_file_get_path(G_FILE(inFile));
    gtk_text_buffer_set_text(inputFileTextBuffer, inFilePath, strlen(inFilePath));
    
    vc_encodingOptions.pInFileStream = inFileStream;
}

void VC_OnInputFileDialogFinished(GObject *fileDialog, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFile *inFile = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(fileDialog), res, &error);
    if (error != NULL)
    {
        VC_PushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    g_file_read_async(inFile, G_PRIORITY_DEFAULT, NULL, VC_FileReadFinished, NULL);
}

void VC_OnChooseFileClicked(GtkFileDialog *fileDialog)
{
    gtk_file_dialog_open(fileDialog, NULL, NULL, VC_OnInputFileDialogFinished, NULL);
    VC_ReadLogQueue();
}

void VC_OnOutputFileDialogFinished(GObject *fileDialog, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GFile *outFile = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(fileDialog), res, &error);
    if (error != NULL)
    {
        VC_PushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    GFileOutputStream *outFileStream = g_file_replace(outFile, NULL, false, G_FILE_CREATE_NONE, NULL, &error);
    if (error != NULL)
    {
        VC_PushLogMessage(error->message, VC_LOG_WARNING);
        g_error_free(error);
        return;
    }

    vc_encodingOptions.pOutFileStream = outFileStream;
    int status = VC_Encode(vc_encodingOptions);
    if (status < 0)
    {
        VC_ReadLogQueue();
        return;
    }
    
    const char *outFilePath = g_file_get_path(G_FILE(outFile));
    gtk_text_buffer_set_text(outputFileTextBuffer, outFilePath, strlen(outFilePath));

    VC_PushLogMessage("File encoded successfully.", VC_LOG_INFO);
    VC_ReadLogQueue();
    
}

void VC_OnConvertClicked(GtkFileDialog *outputFileDialog)
{
    gtk_file_dialog_save(outputFileDialog, NULL, NULL, VC_OnOutputFileDialogFinished, NULL);
    VC_ReadLogQueue();
}

void VC_OnActivate(GtkApplication *app)
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

    GtkWidget *inputFileText = gtk_text_view_new_with_buffer(inputFileTextBuffer);
    GtkWidget *outputFileText = gtk_text_view_new_with_buffer(outputFileTextBuffer);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(inputFileText), false);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(outputFileText), false);

    GtkWidget *grid = gtk_grid_new();
    gtk_window_set_child(GTK_WINDOW(window), grid);
    gtk_grid_attach(GTK_GRID(grid), chooseFileButton, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), convertButton, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), inputFileText, 0, 2, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), outputFileText, 0, 3, 3, 1);
    g_signal_connect_swapped(chooseFileButton, "clicked", G_CALLBACK(VC_OnChooseFileClicked), inputFileDialog);
    g_signal_connect_swapped(convertButton, "clicked", G_CALLBACK(VC_OnConvertClicked), outputFileDialog);
    gtk_window_present(GTK_WINDOW(window));
}

int VC_RunApp(int argc, char** argv)
{
    GtkTextTagTable *textTagTable = gtk_text_tag_table_new();
    inputFileTextBuffer = gtk_text_buffer_new(textTagTable);
    outputFileTextBuffer = gtk_text_buffer_new(textTagTable);

    GtkApplication *app = gtk_application_new("vc.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(VC_OnActivate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);

    return status;
}


#include <gtk/gtk.h>
#include <handy.h>
#include "raider-window.h"
#include "raider-file-row.h"

struct _RaiderWindow
{
    GtkApplicationWindow parent;

    GtkWidget *header_bar;
    GtkWidget *contents_box;
    GtkWidget *shred_button;
    GtkWidget *primary_menu;
    GtkWidget *list_box;
    GtkWidget *window_stack;
    GtkWidget *hide_shredding_check_button;
    GtkWidget *number_of_passes_spin_button;
    GtkWidget *remove_file_check_button;
    GtkWidget *hint_page;
    GtkWidget *status_page;

    GtkCssProvider *provider;
};

G_DEFINE_TYPE (RaiderWindow, raider_window, GTK_TYPE_APPLICATION_WINDOW)

static void
raider_window_init (RaiderWindow *win)
{
    GtkBuilder *builder;
    GMenuModel *menu;

    gtk_widget_init_template(GTK_WIDGET(win));

    builder = gtk_builder_new_from_resource ("/com/github/ADBeveridge/raider/ui/gears-menu.ui");
    menu = G_MENU_MODEL (gtk_builder_get_object (builder, "menu"));
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (win->primary_menu), menu);
    g_object_unref (builder);

    /* Make the window a DND destination. */
    static GtkTargetEntry targetentries[] = {{ "text/uri-list", 0, 0 }};
    gtk_drag_dest_set (GTK_WIDGET(win), GTK_DEST_DEFAULT_ALL, targetentries, 1, GDK_ACTION_COPY); /* Make it into a dnd destination. */
    g_signal_connect (win, "drag_data_received", G_CALLBACK (on_drag_data_received), win);

    raider_window_css_styling(win);

    win->status_page = hdy_status_page_new();
    hdy_status_page_set_icon_name (win->status_page, "document-open-symbolic");
    hdy_status_page_set_title(win->status_page, "Add files or drop here");
    gtk_widget_show_all(win->status_page);
    gtk_box_pack_start(GTK_BOX(win->hint_page), win->status_page, TRUE, TRUE, 0);
}

static void
raider_window_dispose (GObject *object)
{
    G_OBJECT_CLASS (raider_window_parent_class)->dispose (object);
}

void raider_window_css_styling (RaiderWindow *window)
{
    window->provider = gtk_css_provider_new ();
    gtk_style_context_add_provider_for_screen (gdk_screen_get_default(), GTK_STYLE_PROVIDER (window->provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gchar *theme_name = NULL;
    g_object_get (gtk_settings_get_default (), "gtk-theme-name", &theme_name, NULL);


    gchar *theme_uri = g_strconcat ("resource:///org/gnome/raider/theme/", theme_name, ".css", NULL);
    GFile *css_file = g_file_new_for_uri (theme_uri);

    if (g_file_query_exists (css_file, NULL))
    {
        gtk_css_provider_load_from_file (window->provider, css_file, NULL);
    }
    else
        gtk_css_provider_load_from_resource (window->provider, "/com/github/ADBeveridge/raider/theme/Adwaita.css");
}

static void
raider_window_class_init (RaiderWindowClass *class)
{
    G_OBJECT_CLASS (class)->dispose = raider_window_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/com/github/ADBeveridge/raider/ui/raider-window.ui");

    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RaiderWindow, header_bar);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RaiderWindow, primary_menu);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RaiderWindow, shred_button);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RaiderWindow, contents_box);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RaiderWindow, window_stack);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RaiderWindow, list_box);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RaiderWindow, hint_page);

    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), shred_file);
    gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), raider_window_open_file_dialog);
}

/*
 * This is called when the user drops files into the list box.
 */
void on_drag_data_received (GtkWidget *wgt, GdkDragContext *context, gint x, gint y,
                            GtkSelectionData *seldata, guint info, guint time,
                            gpointer data)
{
    RaiderWindow *window = RAIDER_WINDOW(data);

    gchar **filenames = filenames = g_uri_list_extract_uris ( (const gchar *) gtk_selection_data_get_data (seldata) );

    if (filenames == NULL)
    {
        gtk_header_bar_set_subtitle(GTK_HEADER_BAR(window->header_bar), "Unable to add files!");
        gtk_drag_finish (context, FALSE, FALSE, time);
        return;
    }

    int iter = 0;

    while (filenames[iter] != NULL)
    {
        char *filename = g_filename_from_uri (filenames[iter], NULL, NULL);
        raider_window_open(filename, data);

        iter++;
    }

    gtk_drag_finish (context, TRUE, FALSE, time);
}

void
raider_window_open (gchar *filename_to_open, gpointer data)
{
    RaiderWindow *window = RAIDER_WINDOW(data);

    GFile *file = g_file_new_for_path (filename_to_open);
    if (g_file_query_exists (file, NULL) == FALSE)
    {
        gtk_header_bar_set_subtitle(GTK_HEADER_BAR(window->header_bar), "File does not exist!");
        return;
    }
    g_object_unref(file);


    GtkWidget *file_row = GTK_WIDGET(raider_file_row_new(filename_to_open));
    g_signal_connect(file_row, "destroy", G_CALLBACK(raider_window_close), data);
    gtk_container_add(GTK_CONTAINER (window->list_box), file_row);

    gtk_stack_set_visible_child_name(GTK_STACK(window->window_stack), "list_box_page");

    g_free (filename_to_open);
}

void
raider_window_close (gpointer data, gpointer user_data)
{
    RaiderWindow *window = RAIDER_WINDOW(user_data);

    GList *list = gtk_container_get_children(GTK_CONTAINER(window->list_box));
    guint number = g_list_length(list);
    g_list_free(list);

    if (number == 0)
    {
        gtk_stack_set_visible_child_name(GTK_STACK(window->window_stack), "hint_page");
    }
}

void
raider_window_open_file_dialog (GtkWidget *button, RaiderWindow *window)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open File", GTK_WINDOW (window),
                        GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL, "Open",
                        GTK_RESPONSE_ACCEPT, NULL); /* Create dialog. */

    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE); /* Alow to select many files at once. */

    gint res = gtk_dialog_run (GTK_DIALOG (dialog) );
    if (res == GTK_RESPONSE_ACCEPT)
    {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

        GSList *list = gtk_file_chooser_get_filenames (chooser);
        g_slist_foreach (list, (GFunc) raider_window_open, window);
        g_slist_free (list);
    }

    gtk_widget_destroy (dialog);
}


/*
 * THE BIG FUNCTION
 *
 * This function does not use a queue because shred is not cpu intensive, just disk intensive, and
   it is up to the disk to queue io requests.
 */
void shred_file(GtkWidget *widget, gpointer data)
{
    /* Clear the subtitle. */
    RaiderWindow *window = RAIDER_WINDOW(data);
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(window->header_bar), NULL);

    /* Launch the shredding. */
    GList *list = gtk_container_get_children(GTK_CONTAINER(window->list_box));
    guint num = g_list_length(list);
    if (num == 0)
    {
        gtk_header_bar_set_subtitle(GTK_HEADER_BAR(window->header_bar), "No files added!");
    }
    else
    {
        g_list_foreach(list, launch, NULL);
    }
    g_list_free(list);
}

RaiderWindow *
raider_window_new (Raider *app)
{
    return g_object_new (RAIDER_WINDOW_TYPE, "application", app, NULL);
}

/**********************************************************************************/



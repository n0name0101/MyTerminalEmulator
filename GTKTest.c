#include <gtk/gtk.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Buat buffer baru dan set isi dengan "\n\nAsd"
    GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(buffer, "\n1\n", -1);
    g_print("Line Count : %d\n",gtk_text_buffer_get_line_count(buffer));
    // Buat TextView yang menggunakan buffer di atas
    GtkWidget *text_view = gtk_text_view_new_with_buffer(buffer);

    // Dapatkan isi baris kedua (indeks 1)
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_line(buffer, &start, 1);
    end = start;
    gtk_text_iter_forward_to_line_end(&end);
    if (gtk_text_iter_ends_line(&start) == TRUE)
         g_print("Line Kosong \n");

    gint line_length = gtk_text_iter_get_offset(&end) - gtk_text_iter_get_offset(&start);
    gchar *line_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    g_print("Konten baris : '%s'\n Line Length : %d \n", line_text , line_length);
    g_free(line_text);

    // Buat jendela sederhana untuk menampilkan TextView
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_add(GTK_CONTAINER(window), text_view);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}

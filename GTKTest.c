#include <gtk/gtk.h>
#include <string.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Encapsulated Console structure.
typedef struct {
    GtkWidget* window;
    GtkWidget* scrolled_window;
    GtkWidget* text_view;
    GtkTextBuffer* buffer;
    gint y;
    gint x;
} Console;

// Sets the text buffer's cursor to the fixed position and scrolls the view.
void console_set_cursor(Console* console) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
    gtk_text_buffer_place_cursor(console->buffer, &iter);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(console->text_view), &iter, 0.0, FALSE, 0, 0);
}

// Key press callback for Console.
// Handles arrow keys, Ctrl+C, Return, and printable characters (inserted one by one).
static gboolean console_on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    Console* console = (Console*)user_data;

    console_set_cursor(console);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(console->buffer, &iter,
        gtk_text_buffer_get_insert(console->buffer));

    if (event->keyval == GDK_KEY_Left) {
        if (console->x > 0)
            console->x--;
        g_print("Arrow Left: updated fixed cursor to (%d, %d)\n", console->y, console->x);
        console_set_cursor(console);
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Right) {
        console->x++;
        g_print("Arrow Right: updated fixed cursor to (%d, %d)\n", console->y, console->x);
        console_set_cursor(console);
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Up) {
        if (console->y > 0)
            console->y--;
        g_print("Arrow Up: updated fixed cursor to (%d, %d)\n", console->y, console->x);
        console_set_cursor(console);
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Down) {
        console->y++;
        g_print("Arrow Down: updated fixed cursor to (%d, %d)\n", console->y, console->x);
        console_set_cursor(console);
        return TRUE;
    }
    // Detect Control+C.
    else if ((event->state & GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_c)) {
        g_print("Control+C detected.\n");
        return TRUE;
    }
    // Handle Return key: insert newline and update the fixed position.
    else if (event->keyval == GDK_KEY_Return) {
        gtk_text_buffer_insert(console->buffer, &iter, "\n", -1);
        g_print("Inserted newline at (%d, %d)\n", console->y, console->x);
        console->x = 0;
        console->y++;
        console_set_cursor(console);
        return TRUE;
    }
    // For printable characters, insert one by one.
    else if (event->string && strlen(event->string) > 0) {
        size_t len = strlen(event->string);
        for (size_t i = 0; i < len; i++) {
            char ch[2] = { event->string[i], '\0' };
            gtk_text_buffer_insert(console->buffer, &iter, ch, -1);
            g_print("Inserted '%c' at (%d, %d)\n", event->string[i], console->y, console->x);
            console->x++;
            console_set_cursor(console);
        }
    }
    console_set_cursor(console);
    return TRUE;
}

// Initializes the Console structure: creates widgets, sets initial text and fixed cursor,
// and connects event handlers.
void initConsole(Console* console) {
    // Create main window.
    console->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(console->window), WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_window_set_title(GTK_WINDOW(console->window), "Console Text Editor");
    g_signal_connect(console->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create a scrolled window.
    console->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(console->window), console->scrolled_window);

    // Create the text view.
    console->text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(console->text_view), GTK_WRAP_NONE);
    gtk_container_add(GTK_CONTAINER(console->scrolled_window), console->text_view);

    // Get the text buffer.
    console->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->text_view));

    // Set initial text.
    gtk_text_buffer_set_text(console->buffer,
        "Console text insertion demo.\n"
        "Characters will be inserted at a fixed position until you change it.\n",
        -1);

    // Set initial fixed cursor position (starting at line 1, column 0).
    console->y = 1;
    console->x = 0;
    console_set_cursor(console);

    // Connect key-press-event.
    g_signal_connect(console->text_view, "key-press-event", G_CALLBACK(console_on_key_press), console);
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    Console console;
    initConsole(&console);

    gtk_widget_show_all(console.window);
    gtk_main();

    return 0;
}

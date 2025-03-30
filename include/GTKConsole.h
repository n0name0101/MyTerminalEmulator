#pragma once

#include "MyPTY.h"

#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <pango/pango.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define READ_BUFFER_SIZE 512

/* Tambahkan buffer untuk escape sequence ke dalam struktur Console */
typedef struct {
    GtkWidget* window;
    GtkWidget* scrolled_window;
    GtkWidget* text_view;
    GtkTextBuffer* buffer;
    PTY* pty;
    gint y;
    gint x;
    /* Atribut untuk pewarnaan teks */
    const char* current_fg;
    const char* current_bg;
    gboolean bold;
    /* Hash table untuk menyimpan tag berdasarkan kombinasi atribut */
    GHashTable* tags;
    /* Buffer dan panjang untuk escape sequence yang belum lengkap */
    char read_buffer[READ_BUFFER_SIZE];
    int esc_buffer_len;
} Console;

// Function Declaration
void InitConsole(Console* console, PTY* pty);

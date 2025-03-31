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
   gint y;
   gint x;
   int scroll_top;    // Baris awal region scroll (0-based)       ESC[....r
   int scroll_bottom; // Baris akhir region scroll (0-based)      ESC[....r

   /* Atribut untuk pewarnaan teks */
   const char* current_fg;
   const char* current_bg;
   gboolean bold;

   /* Hash table untuk menyimpan tag berdasarkan kombinasi atribut */
   GHashTable* tags;

   /* Buffer dan panjang untuk escape sequence yang belum lengkap */
   char read_buffer[READ_BUFFER_SIZE];
   int esc_buffer_len;

   /* PTY */
   PTY* pty;
   int console_max_cols;
   int console_max_rows;

} Console;

// Function Declaration
void InitConsole(Console* console, PTY* pty);

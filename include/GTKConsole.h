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
void delete_with_cursor_position(Console *console);


// STATIC FUNCTION
// STATIC FUNCTION
// static inline void get_line_offset(Console *console, GtkTextIter *iter, gint y, gint x) {
//     // Pastikan buffer memiliki setidaknya y + 1 baris.
//     gint line_count = gtk_text_buffer_get_line_count(console->buffer);
//     if (y >= line_count) {
//         GtkTextIter end_iter;
//         gtk_text_buffer_get_end_iter(console->buffer, &end_iter);
//         // Sisipkan newline hingga jumlah baris mencapai y + 1.
//         for (gint i = line_count; i <= y; i++) {
//             gtk_text_buffer_insert(console->buffer, &end_iter, "\n", -1);
//         }
//     }
//
//     // Dapatkan iterator awal dan akhir baris yang diinginkan.
//     GtkTextIter line_start, line_end;
//     gtk_text_buffer_get_iter_at_line(console->buffer, &line_start, y);
//     gtk_text_buffer_get_iter_at_line_end(console->buffer, &line_end);
//
//     // Hitung panjang baris.
//     gint line_length = gtk_text_iter_get_offset(&line_end) - gtk_text_iter_get_offset(&line_start);
//     if (x > line_length) {
//         // Sisipkan spasi di akhir baris hingga mencapai offset yang diinginkan.
//         GtkTextIter current;
//         gtk_text_buffer_get_iter_at_line_end(console->buffer, &current);
//         for (gint i = line_length; i < x; i++) {
//             gtk_text_buffer_insert(console->buffer, &current, " ", -1);
//         }
//     }
//
//     // Setelah penyesuaian, dapatkan iter pada baris dan offset yang tepat.
//     gtk_text_buffer_get_iter_at_line_offset(console->buffer, iter, y, x);
// }

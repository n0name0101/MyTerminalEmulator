#include "GTKConsole.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <pango/pango.h>

/* Fungsi helper: Mendapatkan atau membuat GtkTextTag sesuai atribut saat ini. */
static GtkTextTag* get_current_tag(Console* console) {
    char key[128];
    snprintf(key, sizeof(key), "fg=%s;bg=%s;bold=%d", console->current_fg, console->current_bg, console->bold);
    GtkTextTag* tag = g_hash_table_lookup(console->tags, key);
    if (!tag) {
        /* Buat tag baru dengan nama key */
        tag = gtk_text_buffer_create_tag(console->buffer, key, NULL);
        if (g_strcmp0(console->current_fg, "default") != 0) {
            g_object_set(tag, "foreground", console->current_fg, NULL);
        }
        if (g_strcmp0(console->current_bg, "default") != 0) {
            g_object_set(tag, "background", console->current_bg, NULL);
        }
        if (console->bold) {
            g_object_set(tag, "weight", PANGO_WEIGHT_BOLD, NULL);
        }
        /* Simpan tag ke hash table */
        g_hash_table_insert(console->tags, g_strdup(key), tag);
    }
    return tag;
}

/* Mengatur posisi kursor dan melakukan scroll ke posisi tersebut. */
void console_set_cursor(Console* console) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
    gtk_text_buffer_place_cursor(console->buffer, &iter);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(console->text_view), &iter, 0.0, FALSE, 0, 0);
}

/* Fungsi untuk memproses escape sequence SGR (Select Graphic Rendition) untuk pewarnaan teks. */
static void process_sgr(Console* console, const char* seq) {
    if (seq == NULL || strlen(seq) == 0) {
        /* Jika tidak ada parameter, reset atribut. */
        console->current_fg = "default";
        console->current_bg = "default";
        console->bold = FALSE;
        return;
    }
    /* Pecah parameter berdasarkan ';' */
    char* seq_copy = strdup(seq);
    char* token = strtok(seq_copy, ";");
    while (token != NULL) {
        int code = atoi(token);
        if (code == 0) {
            /* Reset semua atribut. */
            console->current_fg = "default";
            console->current_bg = "default";
            console->bold = FALSE;
        }
        else if (code == 1) {
            console->bold = TRUE;
        }
        else if (code >= 30 && code <= 37) {
            /* Kode warna foreground standar. */
            switch (code) {
            case 30: console->current_fg = "black"; break;
            case 31: console->current_fg = "red"; break;
            case 32: console->current_fg = "green"; break;
            case 33: console->current_fg = "yellow"; break;
            case 34: console->current_fg = "blue"; break;
            case 35: console->current_fg = "magenta"; break;
            case 36: console->current_fg = "cyan"; break;
            case 37: console->current_fg = "white"; break;
            }
        }
        else if (code >= 40 && code <= 47) {
            /* Kode warna background standar. */
            switch (code) {
            case 40: console->current_bg = "black"; break;
            case 41: console->current_bg = "red"; break;
            case 42: console->current_bg = "green"; break;
            case 43: console->current_bg = "yellow"; break;
            case 44: console->current_bg = "blue"; break;
            case 45: console->current_bg = "magenta"; break;
            case 46: console->current_bg = "cyan"; break;
            case 47: console->current_bg = "white"; break;
            }
        }
        token = strtok(NULL, ";");
    }
    free(seq_copy);
}

/* Fungsi untuk memproses escape sequence OSC (Operating System Command).
   Contoh format: ESC ] 0 ; <judul> BEL atau ESC ] 0 ; <judul> ESC \ */
static void process_osc(Console* console, const char* osc) {
    /* OSC diharapkan dalam format "0;<judul>" */
    if (strncmp(osc, "0;", 2) == 0) {
        const char* title = osc + 2;
        gtk_window_set_title(GTK_WINDOW(console->window), title);
        g_print("Window title set to: %s\n", title);
    }
}

/* Fungsi untuk memproses escape sequence private mode (ESC[?...)
   Contoh: ESC[?25h/l untuk menampilkan/menyembunyikan kursor. */
static void process_private_mode(Console* console, const char* param, char mode) {
    if (strcmp(param, "25") == 0) {  // Parameter 25 untuk kursor.
        if (mode == 'h') {
            gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(console->text_view), TRUE);
            g_print("Cursor shown\n");
        }
        else if (mode == 'l') {
            gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(console->text_view), FALSE);
            g_print("Cursor hidden\n");
        }
    }
    else {
        g_print("Private mode %s%c not implemented\n", param, mode);
    }
}

/* Thread function untuk membaca data dari PTY. */
static gboolean pty_reader(gpointer data) {
    Console* console = (Console*)data;

    /* Set up file descriptor set dan timeout. */
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(console->pty->master, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;  /* Tidak menunggu, hanya polling */

    /* Cek apakah ada data yang tersedia. */
    int ready = select(console->pty->master + 1, &readfds, NULL, NULL, &tv);
    if (ready > 0 && FD_ISSET(console->pty->master, &readfds)) {

        char* buf = console->read_buffer;
        ssize_t n = read(console->pty->master, buf, sizeof(console->read_buffer));

        if (n > 0) {
            /* Update kursor sebelum memproses data. */
            console_set_cursor(console);
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_mark(console->buffer, &iter,
                gtk_text_buffer_get_insert(console->buffer));

            for (size_t i = 0; i < n; i++) {
                /* Debug print untuk melihat karakter yang diterima */
                g_print("Inserted '%c' with hex %02X\n", buf[i], (unsigned char)buf[i]);

                if (buf[i] == 0x1B) {  /* Escape character */
                    if ((i + 1) < n) {
                        /* OSC: ESC ] ... */
                        if (buf[i + 1] == ']') {
                            i += 2;  /* Lewati ESC dan ']' */
                            char osc[256];
                            int osc_index = 0;
                            while (i < n && osc_index < (int)sizeof(osc) - 1) {
                                if (buf[i] == 0x07) {  /* BEL sebagai terminator */
                                    i++;
                                    break;
                                }
                                if (buf[i] == 0x1B && (i + 1) < n && buf[i + 1] == '\\') {
                                    i += 2;
                                    break;
                                }
                                osc[osc_index++] = buf[i];
                                i++;
                            }
                            osc[osc_index] = '\0';

                            process_osc(console, osc);
                            continue;
                        }
                        /* CSI: ESC [ ... */
                        else if (buf[i + 1] == '[') {
                            /* Cek apakah ini adalah private mode (ESC[?...) */
                            if ((i + 2) < n && buf[i + 2] == '?') {
                                i += 3; /* Lewati ESC, '[' dan '?' */
                                char param[32];
                                int param_index = 0;
                                while (i < n && !isalpha(buf[i]) && param_index < (int)sizeof(param) - 1) {
                                    param[param_index++] = buf[i++];
                                }
                                param[param_index] = '\0';
                                if (i < n) {
                                    char mode = buf[i];  /* Biasanya 'h' atau 'l' */
                                    process_private_mode(console, param, mode);
                                }
                            }
                            else {
                                /* Proses SGR: ESC [ ... m */
                                i += 2; /* Lewati ESC dan '[' */

                                char seq[32];
                                int seq_index = 0;
                                while (i < n && !isalpha(buf[i]) && seq_index < (int)sizeof(seq) - 1) {
                                    seq[seq_index++] = buf[i];
                                    i++;
                                }
                                seq[seq_index] = '\0';

                                if (i < n && buf[i] == 'm') {
                                    process_sgr(console, seq);
                                }
                            }
                        }
                    }
                }
                else if (buf[i] == '\n') {
                    gtk_text_buffer_insert(console->buffer, &iter, "\n", -1);
                    console->x = 0;
                    console->y++;
                }
                else if (buf[i] == '\r') {
                    console->x = 0;
                }
                else if (buf[i] == '\t') {
                    /* Deteksi tab: bisa disisipkan karakter tab atau diubah menjadi spasi.
                       Di sini kita sisipkan karakter tab dan mengupdate posisi x dengan lebar 4. */
                    char input[2] = { '\t', '\0' };
                    GtkTextTag* tag = get_current_tag(console);
                    gtk_text_buffer_insert_with_tags(console->buffer, &iter, input, -1, tag, NULL);
                    console->x += 4;  /* Asumsi lebar tab = 4 karakter */
                }
                else if (isprint(buf[i])) {
                    char input[2] = { buf[i], '\0' };
                    GtkTextTag* tag = get_current_tag(console);
                    gtk_text_buffer_insert_with_tags(console->buffer, &iter, input, -1, tag, NULL);
                    console->x++;
                }
                console_set_cursor(console);
            }
        }
    }
    return TRUE;
}

/* Key press callback untuk Console.
   Menangani arrow keys, Ctrl+C, Return, dan karakter cetak (yang dikirim satu per satu). */
static gboolean console_on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    Console* console = (Console*)user_data;
    if (event->keyval == GDK_KEY_Left) {
        if (console->x > 0)
            console->x--;
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Right) {
        console->x++;
    }
    else if (event->keyval == GDK_KEY_Up) {
        if (console->y > 0)
            console->y--;
    }
    else if (event->keyval == GDK_KEY_Down) {
        console->y++;
    }
    else if ((event->state & GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_c)) {
        g_print("Control+C detected.\n");
    }
    else if (event->keyval == GDK_KEY_Return) {
        write(console->pty->master, "\n", 1);
    }
    else if (event->string && strlen(event->string) > 0) {
        size_t len = strlen(event->string);
        for (size_t i = 0; i < len; i++) {
            write(console->pty->master, &(event->string[i]), 1);
        }
    }
    return TRUE;
}

/* Inisialisasi struktur Console: membuat widget, mengatur teks awal, kursor tetap,
   menghubungkan event handler, dan inisialisasi hash table untuk tag. */
void InitConsole(Console* console, PTY* pty) {
    console->pty = pty;
    console->current_fg = "default";
    console->current_bg = "default";
    console->bold = FALSE;
    console->tags = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    console->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(console->window), WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_window_set_title(GTK_WINDOW(console->window), "Console Text Editor");
    g_signal_connect(console->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    console->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(console->window), console->scrolled_window);

    console->text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(console->text_view), GTK_WRAP_NONE);
    gtk_container_add(GTK_CONTAINER(console->scrolled_window), console->text_view);

    console->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->text_view));
    gtk_text_buffer_set_text(console->buffer, " ", -1);

    console->esc_buffer_len = 0;
    console->y = 0;
    console->x = 0;
    console_set_cursor(console);

    g_signal_connect(console->text_view, "key-press-event", G_CALLBACK(console_on_key_press), console);
    g_idle_add(pty_reader, console);
}

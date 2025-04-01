#include "GTKConsole.h"

static inline void process_osc(Console* console, const char* osc);
static inline void process_private_mode(Console* console, const char* param, char mode);
static inline void process_clear(Console* console, const char* seq);
static inline void process_cursor_position(Console* console, const char* seq);
static inline void process_cursor_right(Console* console, const char* seq);
static inline void process_cursor_up(Console* console, const char* seq);
static inline void process_sgr(Console* console, const char* seq);
static inline void process_scroll_region(Console* console, const char* seq);
static inline void process_erase_line(Console* console, const char* seq);
static inline void process_dsr(Console* console, const char* seq);
static inline void process_insert_line(Console* console, const char* seq);
static inline void scroll_region(Console *console, int scroll_top, int scroll_bottom);
static inline void process_reverse_index(Console* console, const char* seq);
static inline void process_delete(Console* console, const char* seq);

static inline void CSIProcessing(Console* console, char *CSIsequence) {
    size_t len = strlen(CSIsequence);
    char final = CSIsequence[len - 1];
    // Hapus karakter final dari sequence untuk parsing parameter
    CSIsequence[len - 1] = '\0';

    switch (final) {
        case 'm':  // Warna
            process_sgr(console, CSIsequence + 2);
            printf("Color configuration\n");
            break;
        case 'r':  // Scrolling region
            process_scroll_region(console, CSIsequence + 2);
            break;
        case 'M':  // Reverse index: ESC[...M
            process_reverse_index(console, CSIsequence + 2);
            break;
        case 'P':  // Delete: ESC[...P
            process_delete(console, CSIsequence + 2);
            break;
        case 'J':  // Clear Screen
            process_clear(console, CSIsequence + 2);
            break;
        case 'C':  // Move cursor ke kanan
            process_cursor_right(console, CSIsequence + 2);
            break;
        case 'A':  // Move cursor ke atas
            process_cursor_up(console, CSIsequence + 2);
            break;
        case 'K':  // Erase line
            process_erase_line(console, CSIsequence + 2);
            break;
        case 'H':  // Cursor position
            process_cursor_position(console, CSIsequence + 2);
            break;
        case 'n':  // Device Status Report (DSR)
            process_dsr(console, CSIsequence + 2);
            break;
        case 'L':  // Insert line(s)
            process_insert_line(console, CSIsequence + 2);
            break;
        default:
            // Jika karakter kedua adalah '?' berarti mode privat
            if (CSIsequence[2] == '?') {
                char param[32];
                int param_index = 0;
                int i = 3;
                while (!isalpha(CSIsequence[i]) && param_index < (int)sizeof(param) - 1) {
                    param[param_index++] = CSIsequence[i++];
                }
                param[param_index] = '\0';
                char mode = CSIsequence[i];  // Biasanya 'h' atau 'l'
                process_private_mode(console, param, mode);
            }
            else
                g_print("Unknown CSI sequence: %s%c\n", CSIsequence, final);
            break;
    }
}

static inline void process_reverse_index(Console* console, const char* seq) {
    int n = 1;  // Default: scroll 1 baris
    if (strlen(seq) > 0) {
        n = atoi(seq);
        if (n <= 0) {
            n = 1;
        }
    }
    // Lakukan scroll sebanyak n baris dengan menggunakan nilai current y sebagai scroll_top
    for (int i = 0; i < n; i++) {
        scroll_region(console, console->y, console->scroll_bottom);
    }
    g_print("Scroll region executed for %d line(s) via ESC[...M\n", n);
}

// Fungsi untuk memproses escape sequence delete (ESC[...P)
// Menggunakan atoi untuk menentukan jumlah karakter yang dihapus.
static inline void process_delete(Console* console, const char* seq) {
    int n = 1;  // Default: hapus 1 karakter
    if (seq != NULL && strlen(seq) > 0) {
        n = atoi(seq);
        if (n <= 0) {
            n = 1;
        }
    }
    // Lakukan delete sebanyak n karakter dengan menggunakan delete_with_cursor_position(console)
    for (int i = 0; i < n; i++) {
        delete_with_cursor_position(console);
    }
    g_print("Delete executed: %d character(s) deleted via ESC[...P\n", n);
}

// Fungsi untuk melakukan scroll pada scroll region
static inline void scroll_region(Console *console, int scroll_top, int scroll_bottom) {
    GtkTextIter start_iter, end_iter;
    // Dapatkan iterator pada awal baris scroll_top
    gtk_text_buffer_get_iter_at_line(console->buffer, &start_iter, scroll_top);
    // Dapatkan iterator pada awal baris berikutnya
    gtk_text_buffer_get_iter_at_line(console->buffer, &end_iter, scroll_top + 1);
    // Hapus baris pertama dalam region
    gtk_text_buffer_delete(console->buffer, &start_iter, &end_iter);

    // Fill last line with space
    gtk_text_buffer_get_iter_at_line(console->buffer, &start_iter, scroll_bottom);
    gtk_text_buffer_insert(console->buffer, &start_iter, " \n", -1);
    g_print("Scroll region: deleting line at %d\n", console->scroll_top);
}

/* Fungsi untuk memproses escape sequence insert line (ESC[<n>L)
   Fungsi ini akan menyisipkan n baris kosong pada posisi baris saat ini.
   Jika parameter kosong, defaultnya adalah 1 baris. */
static inline void process_insert_line(Console* console, const char* seq) {
    int n = 1;  // Default: insert 1 line
    if (seq != NULL && strlen(seq) > 0) {
        n = atoi(seq);
        if (n <= 0) {
            n = 1;
        }
    }
    // Lakukan penyisipan n baris kosong di posisi kursor.
    for (int i = 0; i < n; i++) {
        GtkTextIter iter;
        GtkTextIter start_iter, end_iter;
        // Hapus baris terakhir
        // Dapatkan iterator pada awal baris scroll_top
        gtk_text_buffer_get_iter_at_line(console->buffer, &start_iter, console->scroll_bottom);
        // Dapatkan iterator pada awal baris berikutnya
        gtk_text_buffer_get_iter_at_line(console->buffer, &end_iter, console->scroll_bottom + 1);
        // Hapus baris pertama dalam region
        gtk_text_buffer_delete(console->buffer, &start_iter, &end_iter);

        // Dapatkan iterator pada awal baris saat ini
        gtk_text_buffer_get_iter_at_line(console->buffer, &iter, console->y);
        // Sisipkan newline, yang akan mendorong baris di bawah ke bawah.
        gtk_text_buffer_insert(console->buffer, &iter, " \n", -1);
    }
    g_print("Inserted %d line(s) at row %d.\n", n, console->y);
}

/* Fungsi untuk memproses escape sequence OSC (Operating System Command).
   Contoh format: ESC ] 0 ; <judul> BEL atau ESC ] 0 ; <judul> ESC \ */
static inline void process_osc(Console* console, const char* osc) {
    /* OSC diharapkan dalam format "0;<judul>" */
    if (strncmp(osc, "0;", 2) == 0) {
        const char* title = osc + 2;
        gtk_window_set_title(GTK_WINDOW(console->window), title);
        g_print("Window title set to: %s\n", title);
    }
}

/* Fungsi untuk memproses escape sequence private mode (ESC[?...)
   Contoh: ESC[?25h/l untuk menampilkan/menyembunyikan kursor. */
static inline void process_private_mode(Console* console, const char* param, char mode) {
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

/* Fungsi untuk memproses escape sequence DSR (Device Status Report)
   Escape sequence ini dapat berupa:
   - ESC[6n: Meminta laporan posisi kursor.
   - ESC[5n: Meminta laporan status terminal (umumnya respon OK adalah ESC[0n).

   Parameter 'seq' berisi karakter di antara '[' dan 'n'. */
static inline void process_dsr(Console* console, const char* seq) {
    if (strcmp(seq, "6") == 0) {
        // ESC[6n: Laporan posisi kursor.
        int row = console->y + 1;  // Konversi ke 1-based
        int col = console->x + 1;  // Konversi ke 1-based
        char response[64];
        snprintf(response, sizeof(response), "\33[%d;%dR", row, col);
        write(console->pty->master, response, strlen(response));
        g_print("Cursor position reported: ");
        for (int i = 0; i < strlen(response); i++) {
           g_print("%02X ", (unsigned char)response[i]);
        }
        g_print("\n");
    }
    else if (strcmp(seq, "5") == 0) {
        // ESC[5n: Laporan status terminal.
        // Biasanya terminal merespons dengan ESC[0n untuk status OK.
        char response[] = "\33[0n";
        write(console->pty->master, response, strlen(response));
        g_print("Terminal status reported: %s\n", response);
    }
    else {
        g_print("Unknown DSR sequence: %s\n", seq);
    }
}

/* Fungsi untuk memproses escape sequence clear (misal: ESC[2J) */
static inline void process_clear(Console* console, const char* seq) {
    /* Escape sequence clear umumnya menggunakan parameter "2" (ESC[2J)
       Untuk memastikan, kita parsing parameter dari seq */
    if (seq != NULL && strlen(seq) > 0) {
        int param = atoi(seq);
        if (param == 2 || param == 3) {
            /* Bersihkan buffer teks */
            console->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->text_view));
            gtk_text_buffer_set_text(console->buffer, " ", -1);
            /* Reset posisi kursor */
            console->x = 0;
            console->y = 0;
            g_print("Screen cleared.\n");
        }
    }
}

/* Fungsi untuk memproses escape sequence cursor position (ESC[<row>;<col>H)
   Baris dan kolom diberikan dalam format 1-based, sehingga perlu dikonversi ke 0-based. */
static inline void process_cursor_position(Console* console, const char* seq) {
    int row = 0, col = 0;
    if (seq != NULL && strlen(seq) > 0) {
        // Misalnya, "12;30" akan menghasilkan row = 12 dan col = 30
        if (sscanf(seq, "%d;%d", &row, &col) == 2) {
            // Konversi dari 1-based ke 0-based
            console->y = (row > 0) ? row - 1 : 0;
            console->x = (col > 0) ? col - 1 : 0;
            g_print("Cursor moved to row %d, column %d\n", console->y, console->x);
        }
        else
            g_print("Invalid cursor position sequence: %s\n", seq);
    }
    else if(seq != NULL && strlen(seq) == 0) {
        console->y = 0;
        console->x = 0;
        g_print("Cursor moved to row %d, column %d\n", console->y, console->x);
    }

    //Make sure the line in buffer is ready to use
    while (gtk_text_buffer_get_line_count(console->buffer) <= console->y) {
        GtkTextIter end_iter;
        gtk_text_buffer_get_end_iter(console->buffer, &end_iter);
        gtk_text_buffer_insert(console->buffer, &end_iter, " \n", -1);
    }
    // Pastikan kolom (console->x) sudah ada pada baris yang dituju
    GtkTextIter line_start, line_end;
    gtk_text_buffer_get_iter_at_line(console->buffer, &line_start, console->y);
    line_end = line_start;
    gtk_text_iter_forward_to_line_end(&line_end);

    // Hitung panjang baris saat ini
    gchar *line_text = gtk_text_buffer_get_text(console->buffer, &line_start, &line_end, FALSE);
    int line_length = strlen(line_text);
    g_free(line_text);

    // Jika panjang baris kurang dari posisi kolom yang diinginkan,
    // langsung sisipkan spasi satu per satu
    while (line_length <= console->x) {
       gtk_text_buffer_insert(console->buffer, &line_end, " ", -1);
       line_length++;
    }
}

static inline void process_cursor_right(Console* console, const char* seq) {
    int n = 1;  // Default: geser kursor ke kanan sebanyak 1 posisi.
    if (seq != NULL && strlen(seq) > 0) {
        n = atoi(seq);
        if (n <= 0) {
            n = 1;
        }
    }
    console->x += n;
    g_print("Cursor moved right by %d positions, new column %d\n", n, console->x);

    // Pastikan baris yang sedang aktif memiliki panjang yang cukup.
    GtkTextIter line_start, line_end;
    gtk_text_buffer_get_iter_at_line(console->buffer, &line_start, console->y);
    line_end = line_start;
    gtk_text_iter_forward_to_line_end(&line_end);

    gchar *line_text = gtk_text_buffer_get_text(console->buffer, &line_start, &line_end, FALSE);
    int line_length = strlen(line_text);
    g_free(line_text);

    // Jika panjang baris kurang dari posisi kolom yang diinginkan, sisipkan spasi.
    while (line_length <= console->x) {
        gtk_text_buffer_insert(console->buffer, &line_end, " ", -1);
        line_length++;
    }
}

// Fungsi untuk memproses escape sequence cursor up (ESC[<n>A)
// Jika parameter kosong, default adalah 1 baris.
static inline void process_cursor_up(Console* console, const char* seq) {
    int n = 1;  // Default: pindah ke atas 1 baris
    if (seq != NULL && strlen(seq) > 0) {
        n = atoi(seq);
        if (n <= 0) {
            n = 1;
        }
    }
    console->y -= n;
    console->x  = 0;
    if (console->y < 0)
        console->y = 0;
    g_print("Cursor moved up by %d line(s), new row: %d\n", n, console->y);
}

/* Fungsi untuk memproses escape sequence SGR (Select Graphic Rendition) untuk pewarnaan teks. */
static inline void process_sgr(Console* console, const char* seq) {
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

static inline void process_scroll_region(Console* console, const char* seq) {
    int top = 0, bottom = 0;
    if (seq != NULL && strlen(seq) > 0) {
        // Misal: "5;20" menghasilkan top = 5 dan bottom = 20 (1-based)
        if (sscanf(seq, "%d;%d", &top, &bottom) == 2) {
            // Simpan sebagai 0-based
            console->scroll_top = (top > 0) ? top - 1 : 0;
            console->scroll_bottom = (bottom > 0) ? bottom - 1 : 0;
            g_print("Scroll region set to: top = %d, bottom = %d\n", console->scroll_top, console->scroll_bottom);
        } else {
            g_print("Invalid scroll region sequence: %s\n", seq);
        }
    }
}

/* Fungsi untuk memproses escape sequence erase line (ESC[K atau ESC[0K)
   Parameter 0 (atau kosong) berarti hapus dari kursor hingga akhir baris.
   Parameter 1 berarti hapus dari awal baris hingga kursor.
   Parameter 2 berarti hapus seluruh baris.
*/
static inline void process_erase_line(Console* console, const char* seq) {
    int param = 0;  // default: 0, erase from cursor to end of line
    if (seq != NULL && strlen(seq) > 0) {
        param = atoi(seq);
    }

    // int line_count = gtk_text_buffer_get_line_count(console->buffer);
    //
    // if (line_count > 0) {
    //     GtkTextIter start_iter, end_iter;
    //     // Dapatkan iterator di awal baris terakhir (indeks: line_count - 1)
    //     gtk_text_buffer_get_iter_at_line(console->buffer, &start_iter, line_count - 1);
    //     // Salin iterator awal ke end_iter
    //     end_iter = start_iter;
    //     // Gerakkan end_iter ke akhir baris
    //     gtk_text_iter_forward_to_line_end(&end_iter);
    //
    //     char *last_line = gtk_text_buffer_get_text(console->buffer, &start_iter, &end_iter, FALSE);
    //     g_print("Last line: %s\n", last_line);
    //     g_free(last_line);
    // } else {
    //     g_print("Buffer kosong.\n");
    // }

    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);

    g_print("Console Y : %d , Console X : %d ",console->y , console->x);
    switch (param) {
        case 0: {
            // Erase from the current cursor position to the end of the line.
            GtkTextIter start_line_offset_iter = iter;
            GtkTextIter end_line_offset_iter = start_line_offset_iter;
            gtk_text_iter_forward_to_line_end(&end_line_offset_iter);
            gtk_text_buffer_delete(console->buffer, &start_line_offset_iter, &end_line_offset_iter);
            break;
        }
        case 1: {
            break;
            // Erase from the beginning of the line to the current cursor position.
            GtkTextIter start_line = iter;
            gtk_text_iter_set_line_offset(&start_line, 0);
            gtk_text_buffer_delete(console->buffer, &start_line, &iter);
            g_print("Erase from beginning of line to cursor.\n");
            break;
        }
        case 2: {
            break;
            // Erase the entire line.
            GtkTextIter start_line = iter, end_line = iter;
            gtk_text_iter_set_line_offset(&start_line, 0);
            gtk_text_iter_forward_to_line_end(&end_line);
            gtk_text_buffer_delete(console->buffer, &start_line, &end_line);
            g_print("Erase entire line.\n");
            break;
        }
        default:
            g_print("Unknown erase line parameter: %d\n", param);
            break;
    }
}

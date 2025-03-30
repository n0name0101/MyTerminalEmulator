#include "GTKConsole.h"

//Static Function Declaration
static void process_osc(Console* console, const char* osc);
static void process_sgr(Console* console, const char* seq);
static void process_clear(Console* console, const char* seq);
static void process_erase_line(Console* console, const char* seq);
static void process_private_mode(Console* console, const char* param, char mode);

typedef enum {
    STATE_NORMAL,    // Tidak dalam escape sequence
    STATE_ESC,       // Sudah terdeteksi ESC (0x1b)
    STATE_CSI,       // Dalam CSI (ESC + '[')
    STATE_OSC,       // Dalam OSC (ESC + ']')
    STATE_OSC_ESC    // Dalam OSC, sudah terdeteksi ESC sebagai potensi awal ST
} State;

int filter_escape_char(Console* console,char input) {
    // Buffer untuk menyimpan escape sequence yang sedang dibangun
    static char esc_buffer[256];
    static int esc_index = 0;
    // Variabel state yang tersimpan antar pemanggilan fungsi
    static State state = STATE_NORMAL;

    switch (state) {
    case STATE_NORMAL:
        if (input == 0x1b) { // Mendeteksi ESC
            state = STATE_ESC;
            esc_index = 0;  // Reset buffer
            esc_buffer[esc_index++] = input;
            return -1;
        }
        else {
            // Bukan bagian escape, langsung kembalikan karakter
            return input;
        }

    case STATE_ESC:
        esc_buffer[esc_index++] = input;
        // Setelah ESC, periksa karakter berikutnya untuk menentukan tipe sequence
        if (input == '[') {
            state = STATE_CSI;
            return -1;
        }
        else if (input == ']') {
            state = STATE_OSC;
            return -1;
        }
        else {
            state = STATE_NORMAL;
            return input;
        }

    case STATE_CSI:
        esc_buffer[esc_index++] = input;
        // Dalam CSI, sequence berakhir jika karakter final berada di rentang 0x40 sampai 0x7E
        if (input >= 0x40 && input <= 0x7E) {
            // CSI sequence selesai, cetak escape sequence yang terdeteksi

            printf("\nDetected CSI: ");
            esc_buffer[esc_index] = '\0';
            if (esc_buffer[strlen(esc_buffer) - 1] == 'm') {
                process_sgr(console, esc_buffer + 2);
                printf("Color configuration\n");
            }
            else if (esc_buffer[strlen(esc_buffer) - 1] == 'J') {
                /* Misalnya, ESC[2J untuk clear screen */
                process_clear(console, esc_buffer + 2);
            }
            else if (esc_buffer[strlen(esc_buffer) - 1] == 'K') {
                // Proses erase line: ESC[K atau ESC[0K
                esc_buffer[strlen(esc_buffer) - 1] = '\0';
                process_erase_line(console, esc_buffer + 2);
            }
            else if (esc_buffer[2] == '?') {
                char param[32];
                int param_index = 0;
                int i = 3;
                while (!isalpha(esc_buffer[i]) && param_index < (int)sizeof(param) - 1) {
                   param[param_index++] = esc_buffer[i++];
                }
                param[param_index] = '\0';
                char mode = esc_buffer[i];  /* Biasanya 'h' atau 'l' */
                process_private_mode(console, param, mode);
            }

            state = STATE_NORMAL;
            esc_index = 0;
        }
        return -1;

    case STATE_OSC:
        esc_buffer[esc_index++] = input;
        // Dalam OSC, sequence berakhir jika menemukan BEL (0x07) atau ST (ESC + '\\')
        if (input == 0x07) {

            printf("\nDetected OSC: ");
            esc_buffer[--esc_index] = '\0';
            process_osc(console, esc_buffer+2);

            state = STATE_NORMAL;
            esc_index = 0;
        }
        else if (input == 0x1b) {
            // Potensi awal dari ST, pindah ke state OSC_ESC
            state = STATE_OSC_ESC;
        }
        return -1;

    case STATE_OSC_ESC:
        esc_buffer[esc_index++] = input;
        if (input == '\\') {
            // OSC sequence selesai (ditemukan ST)
            printf("\nDetected OSC: ");
            for (int i = 0; i < esc_index; i++) {
                putchar(esc_buffer[i]);
            }
            printf("\n");
            state = STATE_NORMAL;
            esc_index = 0;
        }
        else {
            // Jika bukan '\\', kembali ke state OSC
            state = STATE_OSC;
        }
        return -1;

    default:
        state = STATE_NORMAL;
        return input;
    }
}

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

/* Fungsi untuk mengecek apakah kursor terlihat pada GtkTextView */
gboolean is_cursor_visible(GtkTextView *text_view, Console* console) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);

    /* Dapatkan lokasi kursor dalam koordinat widget */
    GdkRectangle cursor_rect;
    gtk_text_view_get_cursor_locations(text_view, &iter, &cursor_rect, NULL);

    /* Dapatkan area tampilan (visible area) dari text view */
    GdkRectangle visible_rect;
    gtk_text_view_get_visible_rect(text_view, &visible_rect);

    visible_rect.height += (int)(visible_rect.height * 0.05);

    /* Cek apakah cursor_rect berada di dalam visible_rect */
    if (cursor_rect.x + cursor_rect.width < visible_rect.x ||
        cursor_rect.x > visible_rect.x + visible_rect.width ||
        cursor_rect.y + cursor_rect.height < visible_rect.y ||
        cursor_rect.y > visible_rect.y + visible_rect.height) {
        return FALSE;
    }
    return TRUE;
}

void scroll_to_cursor(Console* console) {
   GtkTextIter iter;
   gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
   gtk_text_buffer_place_cursor(console->buffer, &iter);

   // Gunakan mark "insert" dan scroll agar mark tersebut terlihat
   GtkTextMark *insert_mark = gtk_text_buffer_get_insert(console->buffer);
   gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(console->text_view), insert_mark);
}

/* Fungsi untuk memproses escape sequence erase line (ESC[K atau ESC[0K)
   Parameter 0 (atau kosong) berarti hapus dari kursor hingga akhir baris.
   Parameter 1 berarti hapus dari awal baris hingga kursor.
   Parameter 2 berarti hapus seluruh baris.
*/
static void process_erase_line(Console* console, const char* seq) {
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
    int line_index = gtk_text_iter_get_line(&iter);
    g_print("Console Y : %d , Console X : %d \n",console->y , console->x);
    switch (param) {
        case 0: {
            // Erase from the current cursor position to the end of the line.
            GtkTextIter start_line_offset_iter = iter;
            GtkTextIter end_line_offset_iter = start_line_offset_iter;
            gtk_text_iter_forward_to_line_end(&end_line_offset_iter);
            gtk_text_buffer_delete(console->buffer, &start_line_offset_iter, &end_line_offset_iter);
            g_print("Erase from cursor to end of line.\n");
            break;
        }
        case 1: {
            // Erase from the beginning of the line to the current cursor position.
            GtkTextIter start_line = iter;
            gtk_text_iter_set_line_offset(&start_line, 0);
            gtk_text_buffer_delete(console->buffer, &start_line, &iter);
            g_print("Erase from beginning of line to cursor.\n");
            break;
        }
        case 2: {
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

/* Fungsi untuk memproses escape sequence clear (misal: ESC[2J) */
static void process_clear(Console* console, const char* seq) {
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

/* Key press callback untuk Console.
   Menangani arrow keys, Ctrl+C, Return, dan karakter cetak (yang dikirim satu per satu). */
static gboolean console_on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    Console* console = (Console*)user_data;

    scroll_to_cursor(console);

    if (event->keyval == GDK_KEY_Left) {
        if (console->x > 0)
            console->x--;
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Right) {
        console->x++;
    }
    else if (event->keyval == GDK_KEY_Up) {
        write(console->pty->master, "\33[A", 3);
    }
    else if (event->keyval == GDK_KEY_Down) {
        console->y++;
    }
    else if ((event->state & GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_c)) {
        write(console->pty->master, "\03", 1);
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

static gboolean pty_channel_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    Console *console = (Console*)data;
    if (condition & G_IO_IN) {
        char *buf = console->read_buffer;
        gsize bytes_read;
        GError *error = NULL;
        GIOStatus status = g_io_channel_read_chars(source, buf, sizeof(console->read_buffer) - 1, &bytes_read, &error);
        if (status == G_IO_STATUS_NORMAL && bytes_read > 0) {
            // Bekukan notifikasi buffer agar update tampilan tidak dipicu tiap kali ada perubahan.
            gtk_text_buffer_begin_user_action(console->buffer);

            GtkTextIter iter;
            // Dapatkan iter dengan koordinat (console->y , console->x)
            gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);

            // Proses seluruh batch data tanpa memanggil update scroll tiap karakter.
            for (gsize i = 0; i < bytes_read; i++) {
                //g_print("Char: '%c', Hex: 0x%02X\n", isprint(buf[i]) ? buf[i] : ' ', (unsigned char)buf[i]);

                if (filter_escape_char(console, buf[i]) == -1) {
                     // Update iter
                     gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                     continue;
                }
                else if (buf[i] == 0x08) {
                    console->x--;
                    g_print("Backspace \n");

                    // Update iter
                    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                }
                else if (buf[i] == '\n') {                                          // Line Feed = 0x0A
                    console->y++;
                    console->x = 0;

                    //Check The Line First
                    int current_line_count = gtk_text_buffer_get_line_count(console->buffer);
                    if (current_line_count == console->y) {
                        GtkTextIter end_iter;
                        gtk_text_buffer_get_end_iter(console->buffer, &end_iter);
                        gtk_text_buffer_insert(console->buffer, &end_iter, "\n", -1);
                    }
                    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                }
                else if (buf[i] == '\r') {                                          // Cariage Return = 0x0D
                    console->x = 0;
                    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                }
                else if (isprint(buf[i])) {
                    char input[2] = { buf[i], '\0' };
                    GtkTextTag* tag = get_current_tag(console);
                    gtk_text_buffer_insert_with_tags(console->buffer, &iter, input, -1, tag, NULL);
                    console->x++;
                }
            }

            // Setelah batch selesai, perbarui cursor dan scrolling satu kali.
            if (is_cursor_visible(GTK_TEXT_VIEW(console->text_view), console))
               scroll_to_cursor(console);

            // Lepaskan beku notifikasi buffer agar tampilan di-update.
            gtk_text_buffer_end_user_action(console->buffer);
        }
        else if (error != NULL) {
            g_error("Error reading from PTY: %s", error->message);
            g_error_free(error);
        }
    }
    if (condition & (G_IO_HUP | G_IO_ERR))
        return FALSE;  // Hentikan watch jika terjadi hangup atau error.
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

    /* Buat CSS provider dan muat data CSS untuk mengatur font TextView */
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "textview { font-family: Monospace; font-size: 11pt; }",
        -1, NULL);

    /* Dapatkan style context dari text view dan tambahkan provider CSS */
    GtkStyleContext *context = gtk_widget_get_style_context(console->text_view);
    gtk_style_context_add_provider(context,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);

    console->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->text_view));
    gtk_text_buffer_set_text(console->buffer, " ", -1);

    console->y = 0;
    console->x = 0;

    g_signal_connect(console->text_view, "key-press-event", G_CALLBACK(console_on_key_press), console);
    //g_timeout_add(50, pty_reader, console);
    //g_idle_add(pty_reader, console);

    GIOChannel *channel = g_io_channel_unix_new(console->pty->master);
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR, pty_channel_callback, console);
}

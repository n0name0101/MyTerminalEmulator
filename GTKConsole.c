#include "GTKConsole.h"
#include "ESCSequence.h"

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
            CSIProcessing(console, esc_buffer);                            // Control Sequence Introducer Processing

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

/* Key press callback untuk Console.
   Menangani arrow keys, Ctrl+C, Return, dan karakter cetak (yang dikirim satu per satu). */
static gboolean console_on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    Console* console = (Console*)user_data;

    if (event->keyval == GDK_KEY_Left) {
        write(console->pty->master, "\33[D", 3);
    }
    else if (event->keyval == GDK_KEY_Right) {
        write(console->pty->master, "\33[C", 3);
    }
    else if (event->keyval == GDK_KEY_Up) {
        write(console->pty->master, "\33[A", 3);
    }
    else if (event->keyval == GDK_KEY_Down) {
        write(console->pty->master, "\33[B", 3);
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
    scroll_to_cursor(console);
    return TRUE;
}

// Fungsi untuk melakukan scroll pada scroll region
static void scroll_region(Console *console) {
    GtkTextIter start_iter, end_iter;
    // Dapatkan iterator pada awal baris scroll_top
    gtk_text_buffer_get_iter_at_line(console->buffer, &start_iter, console->scroll_top);
    // Dapatkan iterator pada awal baris berikutnya
    gtk_text_buffer_get_iter_at_line(console->buffer, &end_iter, console->scroll_top + 1);
    // Hapus baris pertama dalam region
    gtk_text_buffer_delete(console->buffer, &start_iter, &end_iter);
    g_print("Scroll region: deleting line at %d\n", console->scroll_top);
    // Karena satu baris dihapus, sesuaikan posisi kursor jika berada di dalam region
    if (console->y > console->scroll_top)
        console->y--;
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
                g_print("Char: '%c', Hex: 0x%02X\n", isprint(buf[i]) ? buf[i] : ' ', (unsigned char)buf[i]);

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

                    // Jika scroll region telah ditetapkan dan posisi kursor melewati batas bawah region,
                    // lakukan scroll dengan menghapus baris di bagian atas region
                    // (console->scroll_bottom < (console->console_max_rows-1) || console->scroll_top > 0)
                    if (console->scroll_bottom > console->scroll_top && console->y > console->scroll_bottom    \
                        && (console->scroll_bottom < (console->console_max_rows-1) || console->scroll_top > 0) ) {
                       scroll_region(console);
                       gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                       gtk_text_buffer_insert(console->buffer, &iter, " \n", -1);
                       // Setelah scroll, pastikan baris baru telah ditambahkan di bagian bawah jika perlu.
                       // Contoh: jika jumlah baris buffer kurang dari scroll_bottom, tambahkan baris kosong.
                    }

                    //Check The Line First
                    int current_line_count = gtk_text_buffer_get_line_count(console->buffer);
                    if (current_line_count == console->y) {
                        GtkTextIter end_iter;
                        gtk_text_buffer_get_end_iter(console->buffer, &end_iter);
                        gtk_text_buffer_insert(console->buffer, &end_iter, " \n", -1);
                    }
                    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                }
                else if (buf[i] == '\r') {                                          // Cariage Return = 0x0D
                    console->x = 0;
                    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                }
                else if (isprint(buf[i])) {
                    char input[2] = { buf[i], '\0' };

                    // Insert Mode
                    // GtkTextTag* tag = get_current_tag(console);
                    // gtk_text_buffer_insert_with_tags(console->buffer, &iter, input, -1, tag, NULL);

                    // Replace mode
                    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                    GtkTextIter end_iter = iter;
                    gtk_text_iter_forward_to_line_end(&end_iter);

                    // char *last_line = gtk_text_buffer_get_text(console->buffer, &iter, &end_iter, FALSE);
                    // g_print("Console Y = %d : Console X = %d : %s : Length=%ld \n",console->y, console->x,last_line,strlen(last_line));
                    // g_free(last_line);

                    // Hapus karakter yang ada di posisi tersebut (jika ada)
                    if (!gtk_text_iter_equal(&iter,&end_iter)) {
                        gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                        end_iter = iter;
                        gtk_text_iter_forward_char(&end_iter);

                        // char *last_line = gtk_text_buffer_get_text(console->buffer, &iter, &end_iter, FALSE);
                        // g_print("Insert : %c  , Delete Content : Hex: 0x%02X\n", input[0], last_line[0]);
                        // g_free(last_line);

                        gtk_text_buffer_delete(console->buffer, &iter, &end_iter);
                    }

                    gtk_text_buffer_get_iter_at_line_offset(console->buffer, &iter, console->y, console->x);
                    end_iter = iter;

                    // Sisipkan karakter baru dengan tag yang sesuai
                    GtkTextTag* tag = get_current_tag(console);
                    gtk_text_buffer_insert_with_tags(console->buffer, &iter, input, -1, tag, NULL);

                    // Update posisi kursor di struktur Console
                    console->x++;

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

static void on_window_resize(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data) {
    Console *console = (Console *)user_data;

    // Dapatkan allocation dari text_view
    GtkAllocation tv_alloc;
    gtk_widget_get_allocation(console->text_view, &tv_alloc);
    int tv_width = tv_alloc.width;
    int tv_height = tv_alloc.height;

    // Buat layout Pango untuk mengukur ukuran karakter dari text_view.
    PangoLayout *layout = gtk_widget_create_pango_layout(console->text_view, "M");
    if (!layout) {
        return;  // Jika gagal membuat layout, keluar saja
    }

    int char_width, char_height;
    pango_layout_get_pixel_size(layout, &char_width, &char_height);
    g_object_unref(layout);

    if (char_width > 0)
        console->console_max_cols = tv_width / char_width;

    if (char_height > 0)
        console->console_max_rows = tv_height / char_height;

    // g_print("Max cols = %d, max rows = %d\n", console->console_max_cols, console->console_max_rows);

    // Variabel static untuk menyimpan nilai sebelumnya agar ioctl hanya dipanggil saat terjadi perubahan
    static int prev_cols = 0;
    static int prev_rows = 0;

    // Hanya lakukan ioctl jika nilai kolom atau baris telah berubah
    if (console->console_max_cols != prev_cols || console->console_max_rows != prev_rows) {
        struct winsize ws;
        ws.ws_col = console->console_max_cols;
        ws.ws_row = console->console_max_rows;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;

        if (ioctl(console->pty->master, TIOCSWINSZ, &ws) == -1) {
            perror("ioctl TIOCSWINSZ");
        } else {
            g_print("PTY size updated: cols = %d, rows = %d\n", ws.ws_col, ws.ws_row);
        }

        // Update nilai sebelumnya
        prev_cols = console->console_max_cols;
        prev_rows = console->console_max_rows;
        console->scroll_top = 0;
        console->scroll_bottom = console->console_max_rows-1;
    }
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

    // Sambungkan signal "size-allocate" untuk memonitor perubahan ukuran window
    g_signal_connect(console->window, "size-allocate", G_CALLBACK(on_window_resize), console);

    g_signal_connect(console->text_view, "key-press-event", G_CALLBACK(console_on_key_press), console);
    //g_timeout_add(50, pty_reader, console);
    //g_idle_add(pty_reader, console);

    GIOChannel *channel = g_io_channel_unix_new(console->pty->master);
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR, pty_channel_callback, console);
}

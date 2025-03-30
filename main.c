#include "GTKConsole.h"
#include "MyPTY.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//void printToConsole(Console* con) {
//    /* Parent process: read from pty.master in a loop and print its output in hex */
//    char buf[256];
//    int n = read(pty.master, buf, sizeof(buf));
//    if (n > 0) {
//        printf("PTY output in hex: ");
//        for (int i = 0; i < n; i++) {
//            if (buf[i] == '\n') {
//                con->x = 0;
//                con->y++;
//                writeStringLine(&LinesBuffer, con->y, con->x, ' ');
//            }
//            if (buf[i] == '\r') {
//                con->x = 0;
//            }
//            else if (buf[i] == 0x1B) {
//
//            }
//            else {
//                writeStringLine(&LinesBuffer, con->y, con->x, buf[i]);
//                con->x++;
//                printf("%02X ", (unsigned char)buf[i]);
//            }
//        }
//        printf("\n");
//    }
//    /* Adjust vertical scroll offset based on the new cursor position */
//    int total_lines = LinesBuffer.size;
//    if (con->y + 1 > con->max_display_lines)
//        con->scroll_offset_y = (con->y + 1) - con->max_display_lines;
//    else
//        con->scroll_offset_y = 0;
//    redraw(con);
//}

int main(int argc, char* argv[]) {
    // Set up PTY
    const char* shell = "/bin/bash";

    PTY pty;
    Console console;
    gtk_init(&argc, &argv);
    InitPTY(&pty,(char *)shell);
    InitConsole(&console, &pty);

    if(pty.pid != 0) {
        gtk_widget_show_all(console.window);
        gtk_main();
    }
    return 0;
}

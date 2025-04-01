#include "GTKConsole.h"
#include "MyPTY.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

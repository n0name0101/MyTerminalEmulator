#include "MyPTY.h"

void InitPTY(PTY* pty, const char* shell, const char* login) {
    if (openpty(&pty->master, &pty->slave, NULL, NULL, NULL) == -1) {
        perror("openpty");
        exit(EXIT_FAILURE);
    }

    pty->pid = fork();
    if (pty->pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pty->pid == 0) {
        /* Child process: set up the slave as the controlling terminal and exec the shell */
        setsid();
        if (ioctl(pty->slave, TIOCSCTTY, NULL) == -1) {
            perror("ioctl(TIOCSCTTY)");
            exit(EXIT_FAILURE);
        }
        dup2(pty->slave, STDIN_FILENO);
        dup2(pty->slave, STDOUT_FILENO);
        dup2(pty->slave, STDERR_FILENO);
        close(pty->master);
        close(pty->slave);
        execl(shell, login, (char*)NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }
}
# pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pty.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

typedef struct {
    int master, slave;
    pid_t pid;
} PTY;

void InitPTY(PTY* pty, char* shell);

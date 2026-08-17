/* TrigerOS userspace shell (kush) -- runs in ring 3. */
#include "syscall.h"

#define LINE_MAX 128
#define NULL ((void *)0)

static int tstrcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int tstrlen(const char *s)
{
    int n = 0;
    while (*s++) n++;
    return n;
}

static void print(const char *s)
{
    sys_write(1, s, tstrlen(s));
}

static void println(const char *s)
{
    print(s);
    print("\n");
}

static void print_uint(unsigned v)
{
    char buf[12];
    int i = 0;
    if (v == 0) { buf[i++] = '0'; }
    else {
        char tmp[12];
        int j = 0;
        while (v > 0) { tmp[j++] = '0' + (v % 10); v /= 10; }
        while (j > 0) { buf[i++] = tmp[--j]; }
    }
    sys_write(1, buf, i);
}

/* Read a line, return length (excluding \0), -1 on EOF. */
static int readline(char *buf, int maxlen)
{
    int i = 0;
    while (i < maxlen - 1) {
        char ch;
        if (sys_read(0, &ch, 1) <= 0)
            return i > 0 ? i : -1;
        if (ch == '\n' || ch == '\r') {
            sys_write(1, "\n", 1);
            break;
        }
        if (ch == 8 || ch == 127) { /* backspace */
            if (i > 0) {
                i--;
                sys_write(1, " \b", 2);
            }
            continue;
        }
        buf[i++] = ch;
        sys_write(1, &ch, 1);
    }
    buf[i] = 0;
    return i;
}

int kush_main(void)
{
    char line[LINE_MAX];

    while (1) {
        print("kush> ");
        int len = readline(line, LINE_MAX);
        if (len <= 0)
            continue;

        /* skip leading spaces */
        char *cmd = line;
        while (*cmd == ' ') cmd++;
        if (*cmd == 0) continue;

        /* find end of command */
        char *args = cmd;
        while (*args && *args != ' ') args++;
        if (*args) { *args = 0; args++; }
        while (*args == ' ') args++;

        if (tstrcmp(cmd, "help") == 0) {
            println("kush builtins:");
            println("  help    - show this help");
            println("  exit    - exit kush");
            println("  clear   - clear screen");
            println("  echo    - print arguments");
            println("  pid     - show current pid");
            println("  ticks   - show PIT tick count");
            println("  uptime  - show uptime in seconds");
        } else if (tstrcmp(cmd, "exit") == 0) {
            return 0;
        } else if (tstrcmp(cmd, "clear") == 0) {
            print("\033[2J\033[H");
        } else if (tstrcmp(cmd, "echo") == 0) {
            print(args);
            print("\n");
        } else if (tstrcmp(cmd, "pid") == 0) {
            print_uint((unsigned)sys_getpid());
            print("\n");
        } else if (tstrcmp(cmd, "ticks") == 0) {
            print_uint((unsigned)sys_getticks());
            print("\n");
        } else if (tstrcmp(cmd, "uptime") == 0) {
            unsigned sec = (unsigned)sys_getticks() / 100;
            print_uint(sec);
            println("s");
        } else {
            print("unknown: ");
            println(cmd);
        }
    }
}

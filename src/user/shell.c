#include <user.h>
#include <common.h>
#include <stdio.h>
#include <string.h>

void main(void) {
    // yield();
    while (1) {
prompt:
        printf("> ");
        char cmdline[128] = {};
        for (int i = 0;;) {
            char ch = getchar();
            if (i == sizeof(cmdline) - 1) {
                printf("command line too long\n");
                goto prompt;
            } else if (ch == '\r') {
                printf("\n");
                cmdline[i] = '\0';
                break;
            } else if (ch == '\177') {
                if (i == 0) continue;
                cmdline[--i] = '\0';
                printf("\b \b");
            } else if (ch < 31) {
                printf("\n\nUnknown input: 0x%x (%d)\n\n> %S", ch, ch, cmdline);
            } else {
                cmdline[i++] = ch;
                putchar(ch);
            }
        }
#define IS(x) strncmp(cmdline, x, sizeof x) == 0
        if (IS("hello"))
            printf("Hello world from shell!\n");
        else if (IS("exit"))
            exit();
        else if (IS("readfile")) {
            char buf[128];
            int len = readfile("hello.txt", buf, sizeof(buf));
            buf[len] = '\0';
            printf("%S\n", buf);
        }
        else if (IS("writefile"))
            writefile("hello.txt", "Hello from shell!\n", 19);
        else
            printf("unknown command: %S\n", cmdline);
#undef IS
    }
}

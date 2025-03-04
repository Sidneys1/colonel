#define restrict __restrict

extern "C" {
#include <common.h>
#include <stdio.h>
#include <string.h>
#include <user.h>
void main(void);
}

// void *operator new(size_t size) {
//     return (void*)1;
// }

// void delete(void*) {

// }

// class foo {int bar;};


void main(void) {
    // foo bat = {};
    // foo *bar = new foo();
    // delete bar;

    printf("Hello from C++!\n");
    while (1) {
    prompt:
        printf("> ");
        flush();
        // exit();
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
                if (i == 0)
                    continue;
                cmdline[--i] = '\0';
                printf("\b \b");
                flush();
            } else if (ch < 31) {
                printf("\n\nUnknown input: 0x%x (%d)\n\n> %S", ch, ch, cmdline);
                flush();
            } else {
                cmdline[i++] = ch;
                putchar(ch);
                flush();
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
        } else if (IS("writefile"))
            writefile("hello.txt", "Hello from shell!\n", 19);
        else
            printf("unknown command: %S\n", cmdline);
#undef IS
    }
}

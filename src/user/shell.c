#include <user.h>
#include <common.h>
#include <stdio.h>

void main(void) {
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
            // } else if (ch == '\033') {
            //     printf("\nESCAPE part 1: \\033\n");
            //     ch = getchar();
            //     // if (ch == '[') {
            //     //     ch = getchar();
            //     //     printf("\n\nESCAPE: \\033[ 0x%x (%d)\n\n> %s", ch, ch, cmdline);
            //     // }                        
            //     printf("\nESCAPE part 2: \\033 0x%x (%d)\n\n> %s", ch, ch, cmdline);
            } else if (ch < 31) {
                printf("\n\nUnknown input: 0x%x (%d)\n\n> %s", ch, ch, cmdline);
            } else {
                cmdline[i++] = ch;
                putchar(ch);
            }
        }

        if (strcmp(cmdline, "hello") == 0)
            printf("Hello world from shell!\n");
        else if (strcmp(cmdline, "exit") == 0)
            exit();
        else if (strcmp(cmdline, "readfile") == 0) {
            char buf[128];
            int len = readfile("hello.txt", buf, sizeof(buf));
            buf[len] = '\0';
            printf("%s\n", buf);
        }
        else if (strcmp(cmdline, "writefile") == 0)
            writefile("hello.txt", "Hello from shell!\n", 19);
        else
            printf("unknown command: %s\n", cmdline);
    }
}
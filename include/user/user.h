#pragma once

extern inline __attribute__((noreturn)) void exit(void);
extern inline void putchar(char ch);
extern inline int getchar(void);
int readfile(const char *filename, char *buf, int len);
int writefile(const char *filename, const char *buf, int len);

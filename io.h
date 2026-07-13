#ifndef IO_H
#define IO_H
#define FB_GREEN 2
#define FB_DARK_GREY 8

void outb(unsigned short port, unsigned char data);
unsigned char inb(unsigned short port);
int write(char *buf, unsigned int len);

#endif
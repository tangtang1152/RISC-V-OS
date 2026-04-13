#ifndef UACCESS_H
#define UACCESS_H

int copyin(const void *uaddr, void *kaddr, unsigned long len);
int copyout(void *uaddr, const void *kaddr, unsigned long len);
int copyinstr(const char *uaddr, char *kbuf, unsigned long maxlen);

#endif
#ifndef TRAP_H
#define TRAP_H

struct trap_frame {
    unsigned long ra;
    unsigned long sp;

    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long a4;
    unsigned long a5;
    unsigned long a6;
    unsigned long a7;
};

#endif
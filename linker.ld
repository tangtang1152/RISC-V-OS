OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /*
     * We link the kernel at 0x80200000 because this is a common load/entry
     * address when running a RISC-V kernel on QEMU's virt machine with OpenSBI.
     *
     * OpenSBI runs first in a more privileged mode, then transfers control
     * to the next stage (our kernel) at this address.
     */
    . = 0x80200000;

    .text : {
        *(.text.init)
        *(.text .text.*)
    }

    . = ALIGN(4096);
    __userimage_start = .;

    __usertext_start = .;
    .usertext : {
        *(.usertext)
        *(.usertext.*)
    }
    . = ALIGN(4096);
    __usertext_end = .;

    __userrodata_start = .;
    .userrodata : {
        *(.userrodata)
        *(.userrodata.*)
    }
    . = ALIGN(4096);
    __userrodata_end = .;

    __userdata_start = .;
    .userdata : {
        *(.userdata)
        *(.userdata.*)
    }
    . = ALIGN(4096);
    __userdata_end = .;

    __userbss_start = .;
    .userbss : {
        *(.userbss)
        *(.userbss.*)
    }
    . = ALIGN(4096);
    __userbss_end = .;

    __userimage_end = .;

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(.bss.stack)
        *(COMMON)
    }
}
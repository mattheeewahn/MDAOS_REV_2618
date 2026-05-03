ENTRY(mystart)
SECTIONS
{
  /* Load at 0x2000. Code is ~36KB so it ends around 0xB000.
     Stack at 0xC000-0xD000, safely above code and below video RAM. */
  . = 0x2000;
  .text : {
    entry.o(.text)
    *(.text)
    *(.data)
    *(.rodata)
    __bss_start = .;
    *(.bss)
    *(COMMON)
    __bss_end = .;
  }

  /DISCARD/ : {
    *(.eh_frame)
  }

  __stack_bottom = 0xC000;
  __stack_top    = 0xD000;
}

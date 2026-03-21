ENTRY(multiboot_entry)

SECTIONS
{
  . = 2M;

  .multiboot ALIGN(8) :
  {
    KEEP(*(.multiboot))
  }

  .text ALIGN(16) :
  {
    *(.text.boot)
    *(.text*)
  }

  .rodata ALIGN(16) :
  {
    *(.rodata*)
  }

  .data ALIGN(16) :
  {
    *(.data*)
  }

  .bss ALIGN(4096) :
  {
    *(COMMON)
    *(.bss*)
  }

  __kernel_start = 2M;
  __kernel_end = .;
}

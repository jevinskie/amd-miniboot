/* rtl8139-stage0/linker.ld */
ENTRY(_start)

SECTIONS
{
  . = 0x00080000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .bss (NOLOAD) : { *(.bss*) }
}

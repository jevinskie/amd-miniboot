ENTRY(_start)

PHDRS {
  text PT_LOAD FLAGS(5); /* RX */
  data PT_LOAD FLAGS(6); /* RW */
}

SECTIONS {
  . = 0x00080000;

  .text : {
    *(.text*)
  } :text

  .rodata : {
    *(.rodata*)
  } :text

  .data : {
    *(.data*)
  } :data

  .bss (NOLOAD) : {
    __bss_start = .;
    *(.bss*)
    _bss_end = .;
  } :data
}

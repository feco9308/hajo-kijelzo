/* STM32G431CBU6: 128 KiB FLASH. SRAM1 16 KiB + SRAM2 6 KiB
 * contiguous at 0x20000000. Separate 10 KiB CCM at 0x10000000 unused.
 * ST startup copies .data, clears .bss, calls __libc_init_array and main.
 */
ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 128K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 22K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 0x800;
PHDRS
{
  flash PT_LOAD FLAGS(5); /* Read + execute. */
  ram PT_LOAD FLAGS(6);   /* Read + write, data load image resides in flash. */
  zero PT_LOAD FLAGS(6);  /* No flash image for zeroed SRAM and stack reserve. */
}
SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH :flash
  .text : { . = ALIGN(4); *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH
  .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH
  .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH
  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH :ram
  .bss (NOLOAD) : AT(ADDR(.bss)) { . = ALIGN(4); _sbss = .; __bss_start__ = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; __bss_end__ = .; } > RAM :zero
  .stack_reserve (NOLOAD) : AT(ADDR(.stack_reserve)) { . = ALIGN(8); PROVIDE(end = .); PROVIDE(_end = .); . += _Min_Stack_Size; . = ALIGN(8); } > RAM :zero
  .ARM.attributes 0 : { *(.ARM.attributes) } :NONE
}
ASSERT(_ebss + _Min_Stack_Size <= _estack, "SRAM exhausted: stack overlaps data")

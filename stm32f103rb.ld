MEMORY
{
FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 128K
RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{

.text :
{
KEEP(*(.isr_vector))
*(.text*)
*(.rodata*)
_etext = .;
} > FLASH


.data :
{
_sdata = .;
*(.data*)
_edata = .;
} > RAM AT > FLASH

_sidata = LOADADDR(.data);


.bss :
{
_sbss = .;
*(.bss*)
*(COMMON)
_ebss = .;
} > RAM

}

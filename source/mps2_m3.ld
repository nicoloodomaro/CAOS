ìMEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 4M
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 4M
}

SECTIONS
{
    /* 1. Codice e Costanti (in FLASH) */
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        
        /* Inizializzatori C standard */
        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
    } > FLASH

    /* 2. Gestione Eccezioni ARM (Il pezzo che mancava!) */
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        . = ALIGN(4);
    } > FLASH

    /* Marker fine Flash: qui inizia la copia dei dati */
    _etext = .;

    /* 3. Variabili inizializzate (Copiate da FLASH a RAM all'avvio) */
    .data : AT (_etext)
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    /* 4. Variabili a zero (Solo in RAM) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Definizione fine stack */
    _estack = ORIGIN(RAM) + LENGTH(RAM);
}
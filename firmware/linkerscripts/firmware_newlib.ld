/*
 * Generated Linker script file for MK22FN512xxx12
 */

INCLUDE "../linkerscripts/firmware_library_newlib.ld"
INCLUDE "../linkerscripts/firmware_memory.ld"

ENTRY(ResetISR)

SECTIONS
{
    /* MAIN TEXT SECTION */
    .text_start : ALIGN(4)
    {
        FILL(0xff)
        __vectors_start__ = ABSOLUTE(.) ;
        KEEP(*(.isr_vector))
        /* Global Section Table */
        . = ALIGN(4) ;
        __section_table_start = .;
        __data_section_table = .;
        LONG(LOADADDR(.data));
        LONG(    ADDR(.data));
        LONG(  SIZEOF(.data));
        LONG(LOADADDR(.data_RAM2));
        LONG(    ADDR(.data_RAM2));
        LONG(  SIZEOF(.data_RAM2));
        __data_section_table_end = .;
        __bss_section_table = .;
        LONG(    ADDR(.bss));
        LONG(  SIZEOF(.bss));
        LONG(    ADDR(.bss_RAM2));
        LONG(  SIZEOF(.bss_RAM2));
        __bss_section_table_end = .;
        __section_table_end = . ;
        /* End of Global Section Table */

        *(.after_vectors*)
    } > PROGRAM_FLASH

    .codec_bin_section_1 0x4400 : ALIGN(4)
    {
       *(.codec_bin_section_1)
        KEEP(*(.codec_bin_section_1))
    } > PROGRAM_FLASH

    .text_main : ALIGN(4)
    {
       *(EXCLUDE_FILE(*menuCalibration*.o *calibration*.o) .text*)
       *(.rodata .rodata.* .constdata .constdata.*)
       . = ALIGN(4);
       _etext = . ;
    } > PROGRAM_FLASH
    

     

    /* Main DATA section (SRAM_LOWER) */
    .data : ALIGN(4)
    {
       FILL(0xff)
       _data = . ;
       *(vtable)
       *(.ramfunc*)
       *(.data*)
       . = ALIGN(4) ;
       _edata = . ;
    } > SRAM_LOWER AT>PROGRAM_FLASH
    
        /* DATA section for SRAM_LOWER */

    .data_RAM2 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM2 = .) ;
        PROVIDE(__start_data_SRAM_LOWER = .) ;
        *(.ramfunc.$RAM2)
        *(.ramfunc.$SRAM_LOWER)
        *(.data.$RAM2)
        *(.data.$SRAM_LOWER)
        *(.data.$RAM2.*)
        *(.data.$SRAM_LOWER.*)
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM2 = .) ;
        PROVIDE(__end_data_SRAM_LOWER = .) ;
     } > SRAM_LOWER AT>PROGRAM_FLASH
    
    
     
    /* MAIN DATA SECTION */
    .uninit_RESERVED : ALIGN(4)
    {
        KEEP(*(.bss.$RESERVED*))
        . = ALIGN(4) ;
        _end_uninit_RESERVED = .;
    } > SRAM_UPPER

    /* MAIN DATA SECTION */
    .uninit_RESERVED_RAM2 : ALIGN(4)
    {
        KEEP(*(.bss.$RESERVED*))
        . = ALIGN(4) ;
        _end_uninit_RESERVED = .;
    } > SRAM_LOWER

    .codec_bin_section_2 0x54000 : ALIGN(4)
    {
       *(.codec_bin_section_2)
        KEEP(*(.codec_bin_section_2))
    } > PROGRAM_FLASH

    /* Same post-codec application window used by firmware.ld. Explicitly
     * collect language tables; never orphan them into the pre-codec window. */
    .upper_text : ALIGN(4)
    {
        *(.upper_text .upper_text.*)
        *menuCalibration*.o (.text*)
        *calibration*.o (.text*)
        . = ALIGN(4);
    } > PROGRAM_FLASH

    .ARM.extab : ALIGN(4)
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > PROGRAM_FLASH
    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > PROGRAM_FLASH

    /* BSS section for SRAM_LOWER */
    .bss_RAM2 : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM2 = .) ;
       *(.bss.$RAM2*)
       *(.bss.$SRAM_LOWER*)
       *(COMMON)
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM2 = .) ;
    } > SRAM_LOWER


    /* MAIN BSS SECTION */
    .bss : ALIGN(4)
    {
        _bss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4) ;
        _ebss = .;
        PROVIDE(end = .);
    } > SRAM_UPPER

    /* NOINIT section for SRAM_LOWER */
    .noinit_RAM2 (NOLOAD) : ALIGN(4)
    {
       *(.noinit.$RAM2*)
       *(.noinit.$SRAM_LOWER*)
       . = ALIGN(4) ;
    } > SRAM_LOWER 
    

    /* DEFAULT NOINIT SECTION */
    .noinit (NOLOAD): ALIGN(4)
    {
        _noinit = .;
        *(.noinit*) 
         . = ALIGN(4) ;
        _end_noinit = .;
    } > SRAM_UPPER

    /* Reserve and place Heap within memory map */
    _HeapSize = 0x1000;
    .heap :  ALIGN(8)
    {
        _pvHeapStart = .;
        . += _HeapSize;
        . = ALIGN(4);
        _pvHeapLimit = .;
    } > SRAM_LOWER

     _StackSize = 0x1000;
     /* Reserve space in memory for Stack */
    .heap2stackfill  :
    {
        . += _StackSize;
    } > SRAM_LOWER
    
    /* Locate actual Stack in memory map */
    .stack ORIGIN(SRAM_LOWER) + LENGTH(SRAM_LOWER) - _StackSize (NOLOAD) : ALIGN(4)
    {
        _vStackBase = .;
        . = ALIGN(4);
        . += _StackSize;
        _vStackTop = .;
    } > SRAM_LOWER

    /* Provide basic symbols giving location and size of main text
     * block, including initial values of RW data sections. Note that
     * these will need extending to give a complete picture with
     * complex images (e.g multiple Flash banks).
     */
    _image_start = LOADADDR(.text_start);
    _image_end = LOADADDR(.ARM.exidx) + SIZEOF(.ARM.exidx);
    _image_size = _image_end - _image_start;

    ASSERT(ADDR(.text_start) == 0x4000, "application vector base changed")
    ASSERT(ADDR(.text_start) + SIZEOF(.text_start) <= 0x4400, "startup overlaps codec1")
    ASSERT(ADDR(.codec_bin_section_1) == 0x4400, "codec1 address changed")
    ASSERT(ADDR(.text_main) >= 0x5300, "application overlaps codec1")
    ASSERT(ADDR(.codec_bin_section_2) == 0x54000, "codec2 address changed")
    ASSERT(ADDR(.upper_text) == 0x7C090, "high application window overlaps codec2")
    ASSERT(SIZEOF(.codec_bin_section_1) == 0xF00, "unexpected codec1 length")
    ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x54000, "application/data overlap codec2")
    ASSERT(SIZEOF(.data_RAM2) == 0 || LOADADDR(.data_RAM2) + SIZEOF(.data_RAM2) <= 0x54000, "RAM2 load overlaps codec2")
    ASSERT(SIZEOF(.codec_bin_section_2) == 0x28090, "unexpected codec2 length")
    ASSERT(_image_end <= 0x7F000, "image exceeds conservative CPS upload limit")
    ASSERT(_pvHeapLimit <= _vStackBase, "heap overlaps stack")
    ASSERT(ADDR(.heap2stackfill) + SIZEOF(.heap2stackfill) <= _vStackBase, "RAM allocation overlaps stack")

    /* The application image must never carry the bootloader's security field. */
    /DISCARD/ : { *(.FlashConfig) }
}

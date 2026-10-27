/*
 * Copyright 2018 Canaan Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Modified for this sample: reserve 0x805FC000-0x805FFFFF (16 KiB)
 * for the loader, including the corresponding uncached SRAM alias.
 */
MEMORY
{

  ram (wxa!ri) : ORIGIN = 0x80000000, LENGTH = 0x5FC000

  ram_nocache (wxa!ri) : ORIGIN = 0x40000000, LENGTH = 0x5FC000
}

PROVIDE( _ram_start  = ORIGIN(ram) );
PROVIDE( _ram_end    = ORIGIN(ram) + LENGTH(ram) );
PROVIDE( _io_start   = 0x40000000 );
PROVIDE( _io_end     = _io_start + LENGTH(ram) );
PROVIDE( _stack_size = 1 << 15 );

OUTPUT_ARCH( "riscv" )

ENTRY(_start)

PHDRS
{
  ram_ro   PT_LOAD;
  ram_init PT_LOAD;
  ram      PT_NULL;
}

SECTIONS
{

  .text :
  {
    PROVIDE( _text = ABSOLUTE(.) );

    KEEP( *(.text.start) )
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)

    *(.text .text.*)
    *(.gnu.linkonce.t.*)

    . = ALIGN(8);
    PROVIDE( _etext = ABSOLUTE(.) );
  } >ram AT>ram :ram_ro

  .rodata :
  {
    *(.rdata)
    *(.rodata .rodata.*)
    *(.gnu.linkonce.r.*)
  } >ram AT>ram :ram_ro

  . = ALIGN(8);

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >ram AT>ram :ram_ro

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >ram AT>ram :ram_ro

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >ram AT>ram :ram_ro

  .ctors :
  {

    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))

    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  } >ram AT>ram :ram_ro

  .dtors :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  } >ram AT>ram :ram_ro

  . = ALIGN(8);

  .lalign :
  {
    . = ALIGN(8);
    PROVIDE( _data_lma = . );
  } >ram AT>ram :ram_ro

  .dalign :
  {
    . = ALIGN(8);
    PROVIDE( _data = . );
  } >ram AT>ram :ram_init

  . = ALIGN(8);

  .data :
  {

    *(.data .data.*)
    *(.gnu.linkonce.d.*)

    . = ALIGN(8);
    PROVIDE( __global_pointer$ = ABSOLUTE(.) + 0x800);

    *(.sdata .sdata.*)
    *(.gnu.linkonce.s.*)

    . = ALIGN(8);
    *(.srodata.cst16)
    *(.srodata.cst8)
    *(.srodata.cst4)
    *(.srodata.cst2)
    *(.srodata .srodata.*)

    . = ALIGN(64);
    PROVIDE( _edata = ABSOLUTE(.) );
  } >ram AT>ram :ram_init

  .bss :
  {
    PROVIDE( _bss = ABSOLUTE(.) );

    *(.sbss .sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)

    *(.bss .bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)

    . = ALIGN(8);
    PROVIDE( _ebss = ABSOLUTE(.) );
  } >ram AT>ram :ram

  PROVIDE( _tls_data = ABSOLUTE(.) );

  .tdata :
  {
    KEEP( *(.tdata.begin) )
    *(.tdata .tdata.*)
    *(.gnu.linkonce.td.*)
    KEEP( *(.tdata.end) )
  } >ram AT>ram :ram

  .tbss :
  {
    *(.tbss .tbss.*)
    *(.gnu.linkonce.tb.*)
    KEEP( *(.tbss.end) )
  } >ram AT>ram :ram

  . = ALIGN(64);
  PROVIDE( _end = ABSOLUTE(.) );

  PROVIDE( _heap_start = ABSOLUTE(.) + _stack_size * 2 );
  PROVIDE( _tp0 = (_end + 63) & (-64) );
  PROVIDE( _tp1 = _tp0 + _stack_size );
  PROVIDE( _sp0 = _tp0 + _stack_size );
  PROVIDE( _sp1 = _tp1 + _stack_size );

  PROVIDE( _heap_end = _ram_end );
}

ASSERT((_sp0 & 15) == 0 && (_sp1 & 15) == 0, "K210 stacks must be 16-byte aligned")
ASSERT(_stack_size > 0 && (_stack_size & 63) == 0, "K210 stack size must be a positive multiple of 64")
ASSERT(_tp0 >= _end && _tp1 >= _sp0, "K210 stacks overlap program or each other")
ASSERT(_heap_start >= _sp1 && _heap_start <= _heap_end, "K210 heap/stacks exceed RAM")
ASSERT(_heap_end <= ORIGIN(ram) + LENGTH(ram), "K210 heap exceeds RAM")

ASSERT(_heap_start <= _ram_end, "K210 stacks overlap loader")

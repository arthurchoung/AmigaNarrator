/*

 AmigaNarrator

 Copyright (c) 2023 Arthur Choung. All rights reserved.

 Email: arthur -at- hotdoglinux.com

 This file is part of AmigaNarrator.

 AmigaNarrator is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "m68k.h"

void m68k_write_memory_32_no_log(unsigned int addr, unsigned int val);

#define INPUT_BUFSIZE 0x1000
static char *_inputptr = 0;
static char _inputbuf[INPUT_BUFSIZE];

static unsigned char *_library_path = "narrator.device";

#define LIBRARY_BUFSIZE 100000
#define LIBRARY_MAX_HUNKS 8
static unsigned char _library_buf[LIBRARY_BUFSIZE];
static int _library_size = 0;
static int _library_pos = 0;

static int _library_number_of_hunks = 0;
static unsigned int _library_hunk_base[LIBRARY_MAX_HUNKS];

#define MAX_RAM (16*1024*1024)
static unsigned char _ram[MAX_RAM];

static unsigned int _inputbase = 0x28000;
static unsigned int _execbase = 0x20000;
static unsigned int _allocmem = 0x100000;
static unsigned int _narrator_rb = 0x22000;
static unsigned int _msgport = 0x22800;
static unsigned int _audiomsgport = 0x22c00;
static unsigned int _librarybase = 0x23000;
static unsigned int _audiochanbase = 0x24000;
static unsigned int _taskbase = 0x25000;
static int _allocsignal = 31;
static unsigned int _mainbase = 0x26000;
static unsigned int _stackpointer = 0x1f000;
static unsigned int _libraryname = 0x27000;
static unsigned int _addtask = 0;
static unsigned int _makelibrary = 0;

void load_library()
{
    fprintf(stderr, "opening '%s'\n", _library_path);
    FILE *fp = fopen(_library_path, "rb");
    if (!fp) {
        fprintf(stderr, "unable to open '%s'\n", _library_path);
        exit(1);
    }
    int result = fread(_library_buf, 1, LIBRARY_BUFSIZE, fp);
    fprintf(stderr, "fread %d (0x%x)\n", result, result);
    _library_size = result;
    _library_pos = 0;
    fclose(fp);
}

unsigned int library_read_32()
{
    if (_library_pos >= _library_size-3) {
        fprintf(stderr, "library_read_32 past end of file %x\n", _library_pos);
        exit(1);
    }
    uint8_t *p = (uint8_t *) &_library_buf[_library_pos];
    _library_pos += 4;
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p;
    return val;
}

void process_hunks()
{
    unsigned int number_of_hunks = 0;
    unsigned int memory_pos = 0;
    unsigned int hunk_index = 0;
    unsigned int hunk_end = 0;

    unsigned int reloc32_pos = 0;
    unsigned int reloc32_hunk_index = 0;

    for(;;) {
        unsigned int hunk_id = library_read_32();
        if (hunk_id == 0x3f3) {
            fprintf(stderr, "found HUNK_HEADER 0x3f3\n");
            unsigned int zero = library_read_32();
            if (zero != 0) {
                fprintf(stderr, "expecting 0\n");
                exit(1);
            }
            number_of_hunks = library_read_32();
            fprintf(stderr, "number_of_hunks %d\n", number_of_hunks);
            unsigned int first_hunk = library_read_32();
            fprintf(stderr, "first_hunk %d\n", first_hunk);
            unsigned int last_hunk = library_read_32();
            fprintf(stderr, "last_hunk %d\n", last_hunk);
            for (int i=first_hunk; i<=last_hunk; i++) {
                unsigned int hunk_size = library_read_32();
                fprintf(stderr, "hunk %d size 0x%x\n", i, hunk_size);
            }
        } else if (hunk_id == 0x3e9) {
            fprintf(stderr, "found HUNK_CODE 0x3e9\n");
            unsigned int number_of_longwords = library_read_32();
            fprintf(stderr, "number_of_longwords 0x%x\n", number_of_longwords);
            if (hunk_index >= number_of_hunks) {
                fprintf(stderr, "unexpected hunk\n");
                exit(1);
            }
            _library_hunk_base[hunk_index] = memory_pos;
            hunk_index++;
            for (int i=0; i<number_of_longwords; i++) {
                unsigned int val = library_read_32();
                m68k_write_memory_32_no_log(memory_pos, val);
                memory_pos += 4;
            }
        } else if (hunk_id == 0x3ec) {
            fprintf(stderr, "found HUNK_RELOC32 0x3ec\n");
            reloc32_pos = _library_pos;
            for(;;) {
                unsigned int number_of_offsets = library_read_32();
                fprintf(stderr, "number_of_offsets %d\n", number_of_offsets);
                if (!number_of_offsets) {
                    break;
                }
                unsigned int hunk_number = library_read_32();
                fprintf(stderr, "hunk_number %d\n", hunk_number);
                for (int i=0; i<number_of_offsets; i++) {
                    unsigned int offset = library_read_32();
//                    fprintf(stderr, "offset %d 0x%x\n", i, offset);
                }
            }
        } else if (hunk_id == 0x3f2) {
            fprintf(stderr, "found HUNK_END 0x3f2\n");
            hunk_end++;
            if (hunk_end == number_of_hunks) {
                fprintf(stderr, "end of hunks\n");
                break;
            }
        } else if (hunk_id == 0x3ea) {
            fprintf(stderr, "found HUNK_DATA 0x3ea\n");
            unsigned int number_of_longwords = library_read_32();
            fprintf(stderr, "number_of_longwords 0x%x\n", number_of_longwords);
            if (hunk_index >= number_of_hunks) {
                fprintf(stderr, "unexpected hunk\n");
                exit(1);
            }
            _library_hunk_base[hunk_index] = memory_pos;
            hunk_index++;
            for (int i=0; i<number_of_longwords; i++) {
                unsigned int val = library_read_32();
                m68k_write_memory_32_no_log(memory_pos, val);
                memory_pos += 4;
            }
        } else if (hunk_id == 0x3eb) {
            fprintf(stderr, "found HUNK_BSS 0x3eb\n");
            unsigned int number_of_longwords = library_read_32();
            fprintf(stderr, "number_of_longwords 0x%x\n", number_of_longwords);
            if (hunk_index >= number_of_hunks) {
                fprintf(stderr, "unexpected hunk\n");
                exit(1);
            }
            _library_hunk_base[hunk_index] = memory_pos;
            hunk_index++;
            memory_pos += 4*number_of_longwords;
        } else {
            fprintf(stderr, "unhandled hunk type %x\n", hunk_id);
            exit(1);
        }
    }

    if (reloc32_pos) {
        _library_pos = reloc32_pos;
        for(;;) {
            unsigned int number_of_offsets = library_read_32();
            fprintf(stderr, "number_of_offsets %d\n", number_of_offsets);
            if (!number_of_offsets) {
                break;
            }
            unsigned int hunk_number = library_read_32();
            fprintf(stderr, "hunk_number %d\n", hunk_number);
            if (hunk_number >= hunk_index) {
                fprintf(stderr, "hunk_number too high\n");
                exit(1);
            }
            for (int i=0; i<number_of_offsets; i++) {
                unsigned int offset = library_read_32();
//                fprintf(stderr, "reloc32 offset %d 0x%x\n", i, offset);
                unsigned int val = m68k_read_memory_32(offset);
                val += _library_hunk_base[hunk_number];
                m68k_write_memory_32_no_log(offset, val);
            }
        }
    }
}

void process_library_with_romtag()
{
    unsigned int romtagbase = 4;
    fprintf(stderr, "rt_MatchWord 0x4afc\n");
    fprintf(stderr, "rt_MatchTag 0x%x\n", m68k_read_memory_32(romtagbase+2));
    fprintf(stderr, "rt_EndSkip 0x%x\n", m68k_read_memory_32(romtagbase+6));
    unsigned int rt_Flags = m68k_read_memory_8(romtagbase+10);
    fprintf(stderr, "rt_Flags 0x%x\n", rt_Flags);
    unsigned int rtf_AutoInit = 0;
    if (rt_Flags & (1<<7)) {
        rtf_AutoInit = 1;
        fprintf(stderr, "rt_Flags RTF_AUTOINIT\n");
    }
    if (rt_Flags & (1<<2)) {
        fprintf(stderr, "rt_Flags RTF_AFTERDOS\n");
    }
    if (rt_Flags & (1<<1)) {
        fprintf(stderr, "rt_Flags RTF_SINGLETASK\n");
    }
    if (rt_Flags & (1<<0)) {
        fprintf(stderr, "rt_Flags RTF_COLDSTART\n");
    }
    fprintf(stderr, "rt_Version 0x%x\n", m68k_read_memory_8(romtagbase+11));
    fprintf(stderr, "rt_Type 0x%x\n", m68k_read_memory_8(romtagbase+12));
    fprintf(stderr, "rt_Pri 0x%x\n", m68k_read_memory_8(romtagbase+13));
    unsigned int rt_Name = m68k_read_memory_32(romtagbase+14);
    fprintf(stderr, "rt_Name 0x%x '%s'\n", rt_Name, _ram+rt_Name);
    unsigned int rt_IdString = m68k_read_memory_32(romtagbase+18);
    fprintf(stderr, "rt_IdString 0x%x '%s'\n", rt_IdString, _ram+rt_IdString);
    unsigned int rt_Init = m68k_read_memory_32(romtagbase+22);
    fprintf(stderr, "rt_Init 0x%x\n", rt_Init);



    m68k_write_memory_16(_mainbase, 0x4eb9); //jsr
    m68k_write_memory_32(_mainbase+2, rt_Init);

    m68k_write_memory_16(_mainbase+6, 0x7000); //moveq #$0, D0
    m68k_write_memory_16(_mainbase+8, 0x2c7c); //movea.l xxx, A6
    m68k_write_memory_32(_mainbase+10, _librarybase);
    m68k_write_memory_16(_mainbase+14, 0x227c); //movea.l xxx, A1
    m68k_write_memory_32(_mainbase+16, _narrator_rb);

    m68k_write_memory_16(_mainbase+20, 0x4eb9); //jsr
    _makelibrary = _mainbase+22;
    m68k_write_memory_32(_makelibrary, 0); //Open function will be filled in by MakeLibrary

    m68k_write_memory_16(_mainbase+26, 0x23fc); //move.l #$xxxxxxxx, $xxxxxxxx
    m68k_write_memory_32(_mainbase+28, _librarybase);
    m68k_write_memory_32(_mainbase+32, _stackpointer);

    m68k_write_memory_16(_mainbase+36, 0x4eb9); //jsr
    _addtask = _mainbase+38;
    m68k_write_memory_32(_addtask, 0); //address will be filled in by AddTask

    m68k_write_memory_32(_mainbase+42, 0x4e722700); //stop $2700

    m68k_write_memory_8(_narrator_rb+8, 5); // ln_Type NT_MESSAGE
    m68k_write_memory_32(_narrator_rb+14, _msgport);
    m68k_write_memory_16(_narrator_rb+18, 70); // size of narrator_rb (old version)
    m68k_write_memory_32(_narrator_rb+20, _librarybase);

    m68k_set_reg(M68K_REG_SP, _stackpointer);


    m68k_set_reg(M68K_REG_A1, rt_Name);
    m68k_set_reg(M68K_REG_A2, _librarybase);
    m68k_set_reg(M68K_REG_D0, 0);
    m68k_set_reg(M68K_REG_A6, _execbase);
    m68k_set_reg(M68K_REG_PC, _mainbase);
}

void process_library()
{
    if ((_ram[4] == 0x4a) && (_ram[5] == 0xfc)) {
        fprintf(stderr, "ROMTag found\n");
        process_library_with_romtag();
        return;
    }

    fprintf(stderr, "no ROMTag\n");


    m68k_write_memory_16(_mainbase, 0x4eb9); //jsr
    m68k_write_memory_32(_mainbase+2, 0); //library init

    m68k_write_memory_16(_mainbase+6, 0x7000); //moveq #$0, D0
    m68k_write_memory_16(_mainbase+8, 0x2c7c); //movea.l xxx, A6
    m68k_write_memory_32(_mainbase+10, _librarybase);
    m68k_write_memory_16(_mainbase+14, 0x227c); //movea.l xxx, A1
    m68k_write_memory_32(_mainbase+16, _narrator_rb);
    m68k_write_memory_16(_mainbase+20, 0x4eb9); //jsr
    _makelibrary = _mainbase+22;
    m68k_write_memory_32(_makelibrary, 0); //Open function will be filled in by MakeLibrary

    m68k_write_memory_16(_mainbase+26, 0x23fc); //move.l #$xxxxxxxx, $xxxxxxxx
    m68k_write_memory_32(_mainbase+28, _librarybase);
    m68k_write_memory_32(_mainbase+32, _stackpointer);
    m68k_write_memory_16(_mainbase+36, 0x4eb9); //jsr
    _addtask = _mainbase+38;
    m68k_write_memory_32(_addtask, 0); //address will be filled by AddTask

    m68k_write_memory_32(_mainbase+42, 0x4e722700); //stop $2700

    m68k_write_memory_8(_narrator_rb+8, 5); // ln_Type NT_MESSAGE
    m68k_write_memory_32(_narrator_rb+14, _msgport);
    m68k_write_memory_16(_narrator_rb+18, 70); // size of narrator_rb (old version)
    m68k_write_memory_32(_narrator_rb+20, _librarybase);

    m68k_set_reg(M68K_REG_SP, _stackpointer);

    strcpy(_ram+_libraryname, "narrator.device");
    m68k_set_reg(M68K_REG_A1, _libraryname);

    m68k_set_reg(M68K_REG_A2, _librarybase);
    m68k_set_reg(M68K_REG_D0, 0);
    m68k_set_reg(M68K_REG_A6, _execbase);
    m68k_set_reg(M68K_REG_PC, _mainbase);
}

unsigned int m68k_read_memory_8(unsigned int addr)
{
    if (addr >= MAX_RAM) {
        fprintf(stderr, "m68k_read_memory_8 %x OUT OF BOUNDS\n", addr);
        return 0;
    }
    unsigned int val = _ram[addr];
//  fprintf(stderr, "m68k_read_memory_8 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_memory_16(unsigned int addr)
{
    if (addr >= MAX_RAM-1) {
        fprintf(stderr, "m68k_read_memory_16 %x OUT OF BOUNDS\n", addr);
        return 0;
    }
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p;
//  fprintf(stderr, "m68k_read_memory_16 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_memory_32(unsigned int addr)
{
    if (addr >= MAX_RAM-3) {
        fprintf(stderr, "m68k_read_memory_32 %x OUT OF BOUNDS\n", addr);
        return 0;
    }
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p;
//  fprintf(stderr, "m68k_read_memory_32 %x %x\n", addr, val);
    return val;
}

void m68k_write_memory_8(unsigned int addr, unsigned int val)
{
    if (addr >= MAX_RAM) {
        fprintf(stderr, "m68k_read_memory_8 %x OUT OF BOUNDS\n", addr);
        return;
    }
    fprintf(stderr, "m68k_write_memory_8 addr %x val %x\n", addr, val);
    _ram[addr] = val;
}

void m68k_write_memory_16(unsigned int addr, unsigned int val)
{
    if (addr >= MAX_RAM-1) {
        fprintf(stderr, "m68k_read_memory_16 %x OUT OF BOUNDS\n", addr);
        return;
    }
    fprintf(stderr, "m68k_write_memory_16 addr %x val %x\n", addr, val);
    uint8_t *p = (uint8_t *) &_ram[addr];
    p[1] = val&0xff;
    val >>= 8;
    p[0] = val&0xff;
}

void m68k_write_memory_32(unsigned int addr, unsigned int val)
{
    if (addr >= MAX_RAM-3) {
        fprintf(stderr, "m68k_read_memory_32 %x OUT OF BOUNDS\n", addr);
        return;
    }
    fprintf(stderr, "m68k_write_memory_32 addr %x val %x\n", addr, val);
    uint8_t *p = (uint8_t *) &_ram[addr];
    p[3] = val&0xff;
    val >>= 8;
    p[2] = val&0xff;
    val >>= 8;
    p[1] = val&0xff;
    val >>= 8;
    p[0] = val&0xff;
}

void m68k_write_memory_32_no_log(unsigned int addr, unsigned int val)
{
    if (addr >= MAX_RAM-3) {
        fprintf(stderr, "m68k_read_memory_32 %x OUT OF BOUNDS\n", addr);
        return;
    }
    uint8_t *p = (uint8_t *) &_ram[addr];
    p[3] = val&0xff;
    val >>= 8;
    p[2] = val&0xff;
    val >>= 8;
    p[1] = val&0xff;
    val >>= 8;
    p[0] = val&0xff;
}

unsigned int m68k_read_disassembler_8(unsigned int addr)
{
    if (addr >= MAX_RAM) {
        fprintf(stderr, "m68k_read_disassembler_8 %x OUT OF BOUNDS\n", addr);
        return 0;
    }
    unsigned int val = _ram[addr];
//  fprintf(stderr, "m68k_read_disassembler_8 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_disassembler_16(unsigned int addr)
{
    if (addr >= MAX_RAM-1) {
        fprintf(stderr, "m68k_read_disassembler_16 %x OUT OF BOUNDS\n", addr);
        return 0;
    }
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p;
//  fprintf(stderr, "m68k_read_disassembler_16 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_disassembler_32(unsigned int addr)
{
    if (addr >= MAX_RAM-3) {
        fprintf(stderr, "m68k_read_disassembler_32 %x OUT OF BOUNDS\n", addr);
        return 0;
    }
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p;
//  fprintf(stderr, "m68k_read_disassembler_32 %x %x\n", addr, val);
    return val;
}

void make_hex(char *buf, unsigned int pc, unsigned int len)
{
	char *p = buf;
    for (int i=0; i<len; i+=2) {
        if (i > 0) {
			*p++ = ' ';
        }
		sprintf(p, "%04x", m68k_read_memory_16(pc));
		pc += 2;
		p += 4;
	}
}

void instr_hook_callback(unsigned int pc)
{
	char buf[256];
	char buf2[256];

    unsigned int sp = m68k_get_reg(0, M68K_REG_SP);

	unsigned int instr_size = m68k_disassemble(buf, pc, M68K_CPU_TYPE_68000);
	make_hex(buf2, pc, instr_size);
    {
        unsigned int a0 = m68k_get_reg(0, M68K_REG_A0);
        unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
        unsigned int a2 = m68k_get_reg(0, M68K_REG_A2);
        unsigned int a3 = m68k_get_reg(0, M68K_REG_A3);
        unsigned int a4 = m68k_get_reg(0, M68K_REG_A4);
        unsigned int a5 = m68k_get_reg(0, M68K_REG_A5);
        unsigned int a6 = m68k_get_reg(0, M68K_REG_A6);
        fprintf(stderr, "Execute %03x: %-20s: %s (SP=%x A0=%x A1=%x A2=%x A3=%x A4=%x A5=%x A6=%x)\n", pc, buf2, buf, sp, a0, a1, a2, a3, a4, a5, a6);
    }
    unsigned int instr = m68k_read_memory_16(pc);
    if (instr == 0x4eae) { //jsr
        if (instr_size == 4) {
            unsigned int a6 = m68k_get_reg(0, M68K_REG_A6);
            unsigned int arg = m68k_read_memory_16(pc+2);
            fprintf(stderr, "***** JSR %x A6=%x 4=%x\n", arg, a6, m68k_read_memory_32(4));
            m68k_write_memory_16(0x10000+arg, 0x4e75); // rts
            m68k_set_reg(M68K_REG_A6, _execbase);
            if (arg == 0xff3a) { // AllocMem -$c6
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0); // byteSize
                unsigned int d1 = m68k_get_reg(0, M68K_REG_D1); // attributes
                fprintf(stderr, "***** AllocMem byteSize %x attributes %x _allocmem %x\n", d0, d1, _allocmem);
                m68k_set_reg(M68K_REG_D0, _allocmem);
                if (d0 % 4 != 0) {
                    d0 /= 4;
                    d0++;
                    d0 *= 4;
                }
                _allocmem += d0;
            } else if (arg == 0xfeb6) { // AllocSignal -$14a
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0); // signalNum
                fprintf(stderr, "***** AllocSignal signalNum %x _allocsignal %x\n", d0, _allocsignal);
                m68k_set_reg(M68K_REG_D0, _allocsignal);
                // should check to see signal is available
                _allocsignal--;
            } else if (arg == 0xfeda) { // FindTask -$126
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                fprintf(stderr, "***** FindTask %x '%s'\n", a1, (a1) ? ((char *)(_ram+a1)) : "(a1 is 0)");
                m68k_set_reg(M68K_REG_D0, _taskbase);
            } else if (arg == 0xfee6) { // AddTask -$11a
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1); // task
                unsigned int a2 = m68k_get_reg(0, M68K_REG_A2); // initialPC
                unsigned int a3 = m68k_get_reg(0, M68K_REG_A3); // finalPC
                fprintf(stderr, "***** AddTask task %x initialPC %x finalPC %x\n", a1, a2, a3);
                m68k_set_reg(M68K_REG_D0, _taskbase);
                m68k_write_memory_32(_addtask, a2); //set the jsr addr in _mainbase
            } else if (arg == 0xffac) { // MakeLibrary -$54
                unsigned int a0 = m68k_get_reg(0, M68K_REG_A0); // vectors
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1); // structure
                unsigned int a2 = m68k_get_reg(0, M68K_REG_A2); // init
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0); // dSize
                unsigned int d1 = m68k_get_reg(0, M68K_REG_D1); // segList
                fprintf(stderr, "***** MakeLibrary vectors %x structure %x init %x dSize %x segList %x\n", a0, a1, a2, d0, d1);
                m68k_set_reg(M68K_REG_D0, _librarybase);

                unsigned int vectorbase = a0;
                for (int i=0; i<8; i++) {
                    unsigned int vector = m68k_read_memory_32(vectorbase+i*4);
                    if (vector == 0xffffffff) {
                        fprintf(stderr, "end of vectors\n");
                        break;
                    }
                    fprintf(stderr, "vector[%d] = %x\n", i, vector);
                    if (i == 0) {
                        fprintf(stderr, "openfunc %x\n", vector);
                        m68k_write_memory_32(_makelibrary, vector); //set the jsr addr in _mainbase
                    }
                }
                
            } else if (arg == 0xfe50) { // AddDevice -$1b0
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1); // device
                fprintf(stderr, "***** AddDevice %x\n", a1);
            } else if (arg == 0xfe44) { // OpenDevice -$1bc
                unsigned int a0 = m68k_get_reg(0, M68K_REG_A0);
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0);
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                unsigned int d1 = m68k_get_reg(0, M68K_REG_D1);
                fprintf(stderr, "***** OpenDevice devName %x '%s' unit %x ioRequest %x flags %x\n", a0, _ram+a0, d0, a1, d1);
                m68k_set_reg(M68K_REG_D0, 0);
                m68k_write_memory_32(a1+14, _audiomsgport);
            } else if (arg == 0xfe92) { // PutMsg -$16e
                unsigned int a0 = m68k_get_reg(0, M68K_REG_A0);
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                fprintf(stderr, "***** PutMsg port %x message %x\n", a0, a1);
            } else if (arg == 0xfe38) {
                // DoIO -$1c8
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                fprintf(stderr, "***** DoIO ioRequest %x\n", a1);
                unsigned int io_Unit = m68k_read_memory_32(a1+24);
                fprintf(stderr, "***** DoIO io_Unit %x\n", io_Unit);
                unsigned int io_Command = m68k_read_memory_16(a1+28);
                fprintf(stderr, "***** DoIO io_Command %x\n", io_Command);
                fprintf(stderr, "***** DoIO io_Flags %x\n", m68k_read_memory_8(a1+30));
                fprintf(stderr, "***** DoIO io_Error %x\n", m68k_read_memory_8(a1+31));
                unsigned int ioa_Data = m68k_read_memory_32(a1+34);
                fprintf(stderr, "***** DoIO ioa_Data %x\n", ioa_Data);
                unsigned int ioa_Length = m68k_read_memory_32(a1+38);
                fprintf(stderr, "***** DoIO ioa_Length %x\n", ioa_Length);
                unsigned int ioa_Period = m68k_read_memory_16(a1+42);
                fprintf(stderr, "***** DoIO ioa_Period %x\n", ioa_Period);
                unsigned int ioa_Volume = m68k_read_memory_16(a1+44);
                fprintf(stderr, "***** DoIO ioa_Volume %x\n", ioa_Volume);
                unsigned int ioa_Cycles = m68k_read_memory_16(a1+46);
                fprintf(stderr, "***** DoIO ioa_Cycles %x\n", ioa_Cycles);
                if (io_Command == 6) { //CMD_STOP
                    fprintf(stderr, "***** DoIO CMD_STOP\n");
                } else if (io_Command == 7) { //CMD_START
                    fprintf(stderr, "***** DoIO CMD_START\n");
                } else if (io_Command == 9) { //ADCMD_FREE
                    fprintf(stderr, "***** DoIO ADCMD_FREE mn_ReplyPort %x\n", m68k_read_memory_32(a1+14));
                    fprintf(stderr, "***** DoIO ADCMD_FREE io_Device %x\n", m68k_read_memory_32(a1+20));
                    m68k_write_memory_8(a1+31, 0);
                }
                m68k_set_reg(M68K_REG_D0, 0);
            } else if (arg == 0xfebc) { // Signal -$144
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0);
                fprintf(stderr, "***** Signal task %x signalSet %x\n", a1, d0);
            } else if (arg == 0xfe86) { // ReplyMsg -$17a
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                fprintf(stderr, "***** ReplyMsg message %x\n", a1);
                fprintf(stderr, "***** io_Error %x\n", m68k_read_memory_8(_narrator_rb+31));
                exit(1);
            } else if (arg == 0xfe8c) { // GetMsg -$174
                unsigned int a0 = m68k_get_reg(0, M68K_REG_A0);
                fprintf(stderr, "***** GetMsg port %x\n", a0);
                int len = strlen(_inputptr);
                if (len >= INPUT_BUFSIZE) {
                    len = INPUT_BUFSIZE;
                }
                strncpy(_ram+_inputbase, _inputptr, INPUT_BUFSIZE);
                m68k_write_memory_16(_narrator_rb+28, 3); // CMD_WRITE 3 //io_Command
                m68k_write_memory_32(_narrator_rb+44, 0); //io_Offset
                m68k_write_memory_32(_narrator_rb+40, _inputbase); //io_Data
                m68k_write_memory_32(_narrator_rb+36, len); //io_length
                m68k_write_memory_16(_narrator_rb+48, 150); //rate
                m68k_write_memory_16(_narrator_rb+50, 110); //pitch
                m68k_write_memory_16(_narrator_rb+52, 0); //mode 0 natural 1 robotic 2 manual
                m68k_write_memory_16(_narrator_rb+54, 0); //sex 0 male 1 female
                m68k_write_memory_16(_narrator_rb+62, 64); //volume 0-64
                m68k_write_memory_16(_narrator_rb+64, 22200); //sampfreq

                m68k_write_memory_8(_audiochanbase, 3);//not necessary to have all these values
                m68k_write_memory_8(_audiochanbase, 5);
                m68k_write_memory_8(_audiochanbase, 10);
                m68k_write_memory_8(_audiochanbase, 12);
                m68k_write_memory_32(_narrator_rb+56, _audiochanbase);//ch_masks
                m68k_write_memory_16(_narrator_rb+60, 4);//nm_masks
                m68k_write_memory_16(_narrator_rb+31, 0); //io_Error

                m68k_set_reg(M68K_REG_D0, _narrator_rb);
            } else if (arg == 0xfec2) { // Wait -$13e
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0);
                fprintf(stderr, "***** Wait signalSet %x\n", d0);
                unsigned int a2 = m68k_get_reg(0, M68K_REG_A2);
                fprintf(stderr, "***** Wait A2 %x\n", a2);
                fprintf(stderr, "***** Wait A2+0x22 %x\n", a2+0x22);
                fprintf(stderr, "***** Wait (A2+0x22) %x\n", m68k_read_memory_32(a2+0x22));
                m68k_set_reg(M68K_REG_A2, _librarybase);
            } else if (arg == 0xffe2) { // device BeginIO -$1e
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                fprintf(stderr, "***** device BeginIO %x\n", a1);
                unsigned int io_Unit = m68k_read_memory_32(a1+24);
                fprintf(stderr, "***** BeginIO io_Unit %x\n", io_Unit);
                unsigned int io_Command = m68k_read_memory_16(a1+28);
                fprintf(stderr, "***** BeginIO io_Command %x\n", io_Command);
                fprintf(stderr, "***** BeginIO io_Flags %x\n", m68k_read_memory_8(a1+30));
                fprintf(stderr, "***** BeginIO io_Error %x\n", m68k_read_memory_8(a1+31));
                unsigned int ioa_Data = m68k_read_memory_32(a1+34);
                fprintf(stderr, "***** BeginIO ioa_Data %x\n", ioa_Data);
                unsigned int ioa_Length = m68k_read_memory_32(a1+38);
                fprintf(stderr, "***** BeginIO ioa_Length %x\n", ioa_Length);
                unsigned int ioa_Period = m68k_read_memory_16(a1+42);
                fprintf(stderr, "***** BeginIO ioa_Period %x\n", ioa_Period);
                unsigned int ioa_Volume = m68k_read_memory_16(a1+44);
                fprintf(stderr, "***** BeginIO ioa_Volume %x\n", ioa_Volume);
                unsigned int ioa_Cycles = m68k_read_memory_16(a1+46);
                fprintf(stderr, "***** BeginIO ioa_Cycles %x\n", ioa_Cycles);
                for (int i=0; i<ioa_Length; i++) {
                    fprintf(stderr, "***** BeginIO ioa_Data %d %x\n", i, m68k_read_memory_8(ioa_Data+i));
                }
                if (io_Command == 32) {//ADCMD_ALLOCATE
                    fprintf(stderr, "***** BeginIO ADCMD_ALLOCATE\n");
                    m68k_write_memory_8(a1+31, 0);
                    m68k_write_memory_32(a1+24, 0x8/*0xc*/);//io_Unit
                    m68k_write_memory_16(a1+32, 0xaaaa);//ioa_AllocKey
                } else if (io_Command = 3) {//CMD_WRITE
                    fprintf(stderr, "***** BeginIO CMD_WRITE\n");
                    write(1, _ram+ioa_Data, ioa_Length);
                }
            } else if (arg == 0xfe26) { // WaitIO
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                fprintf(stderr, "***** WaitIO %x\n", a1);
                m68k_set_reg(M68K_REG_D0, 0);
            } else if (arg == 0xff2e) { // FreeMem
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0);
                fprintf(stderr, "***** FreeMem memoryBlock %x byteSize %x\n", a1, d0);
            } else if (arg == 0xfed4) { // SetTaskPri
                unsigned int a1 = m68k_get_reg(0, M68K_REG_A1);
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0);
                fprintf(stderr, "***** SetTaskPri task %x priority %x\n", a1, d0);
            } else if (arg == 0xfeb0) { // FreeSignal
                unsigned int d0 = m68k_get_reg(0, M68K_REG_D0);
                fprintf(stderr, "***** FreeSignal signalNum %x\n", d0);
            } else {
                fprintf(stderr, "unhandled\n");
                exit(1);
            }
        }
    } else if (instr == 0x4e72) {
        fprintf(stderr, "***** Stop\n");
        exit(1);
    }
}

void main(int argc, char **argv)
{
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            if (i+1 < argc) {
                _library_path = argv[i+1];
                i++;
            } else {
                fprintf(stderr, "error, expecting path for -d\n");
                exit(1);
            }
        } else if (!strcmp(argv[i], "-")) {
            fprintf(stderr, "reading first line from stdin\n");
            if (!fgets(_inputbuf, INPUT_BUFSIZE, stdin)) {
                fprintf(stderr, "no input\n");
                exit(1);
            }
            int len = strlen(_inputbuf);
            if (len > 0) {
                if (_inputbuf[len-1] == '\n') {
                    _inputbuf[len-1] = 0;
                }
            }
            _inputptr = _inputbuf;
        } else {
            _inputptr = argv[i];
        }
    }

    if (!_inputptr) {
        fprintf(stderr, "Usage: %s [-d narrator_device_file] <-|phonetic_text>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "%s \"/HEH4LOW WER4LD.\"\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~1.0 \"/HEH4LOW WER4LD.\"\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~1.1 \"/HEH4LOW WER4LD.\"\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~1.2 \"/HEH4LOW WER4LD.\"\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~2.04 \"/HEH4LOW WER4LD.\"\n", argv[0]);
        
        fprintf(stderr, "\n");
        fprintf(stderr, "# read from stdin\n");
        fprintf(stderr, "%s -\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~1.0 -\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~1.1 -\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~1.2 -\n", argv[0]);
        fprintf(stderr, "%s -d narrator.device~2.04 -\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "PCM samples will be written to stdout.\n");
        fprintf(stderr, "The format is S8 (signed 8-bit) at 22200 Hz\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "If using Linux, play using ALSA: aplay -f S8 -r 22200\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Redirect stderr to /dev/null to speed up process\n");

        exit(1);
    }


    for (int i=0; i<MAX_RAM; i++) {
        _ram[i] = 0;
    }
    load_library();

    m68k_init();
    m68k_set_instr_hook_callback(instr_hook_callback);
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();

    process_hunks(_library_size);
    process_library();

    for(;;) {
        m68k_execute(100000);
    }

    exit(0);
}


/*
#define CMD_INVALID	0
#define CMD_RESET	1
#define CMD_READ	2
#define CMD_WRITE	3
#define CMD_UPDATE	4
#define CMD_CLEAR	5
#define CMD_STOP	6
#define CMD_START	7
#define CMD_FLUSH	8

#define CMD_NONSTD	9
*/

/*
narrator_rb
0 struct Message - struct Node mn_Node - struct Node *ln_Succ
4 struct Message - struct Node mn_Node - struct Node *ln_Pred
8 struct Message - struct Node mn_Node - UBYTE ln_Type
9 struct Message - struct Node mn_Node - UBYTE ln_Pri
10 struct Message - struct Node mn_Node - char *ln_Name
14 struct Message - struct MsgPort *mn_ReplyPort
18 struct Message - UWORD mn_Length
20 struct IOStdReq - struct Device *io_Device
24 struct IOStdReq - struct Unit *io_Unit
28 struct IOStdReq - UWORD io_Command
30 struct IOStdReq - UBYTE io_Flags
31 struct IOStdReq - BYTE io_Error
    end of IORequest
32 struct IOStdReq - ULONG io_Actual
36 struct IOStdReq - ULONG io_Length
40 struct IOStdReq - APTR io_Data
44 struct IOStdReq - ULONG io_Offset
48 UWORD rate
50 UWORD pitch
52 UWORD mode
54 UWORD sex
56 UBYTE *ch_masks
60 UWORD nm_masks
62 UWORD volume
64 UWORD sampfreq
66 UBYTE mouths
67 UBYTE chanmask
68 UBYTE numchan
69 UBYTE flags
70 UBYTE F0enthusiasm
71 UBYTE F0perturb
72 BYTE F1adj
73 BYTE F2adj
74 BYTE F3adj
75 BYTE A1adj
76 BYTE A2adj
77 BYTE A3adj
78 UBYTE articulate
79 UBYTE centralize
80 char *centphon
84 BYTE AVbias
85 BYTE AFbias
86 BYTE priority
87 BYTE pad1
*/


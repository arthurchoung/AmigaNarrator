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

#include "m68k.h"

static unsigned char *_library_path = "translator.library";

#define LIBRARY_BUFSIZE 100000
static unsigned char _library_buf[LIBRARY_BUFSIZE];
static int _library_size = 0;

#define MAX_RAM (1024*1024)
static unsigned char _ram[MAX_RAM];

static unsigned int _librarybase = 0x4000;
static unsigned int _inputbase = 0x5000;
static unsigned int _outputbase = 0x6000;
static unsigned int _stackpointer = 0xf000;
static unsigned int _mainbase = 0x7000;

#define INPUT_BUFSIZE 0x1000
#define OUTPUT_BUFSIZE 0x1000

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
    fclose(fp);
}

void copy_library_to_ram()
{
    // hardcoded, should parse the hunks
    for (int i=0x24; i<_library_size; i++) {
        _ram[i-0x24] = _library_buf[i];
    }
}

void process_library_with_romtag(char *str)
{
    fprintf(stderr, "rt_MatchWord 0x4afc\n");
    fprintf(stderr, "rt_MatchTag 0x%x\n", m68k_read_memory_32(2));
    fprintf(stderr, "rt_EndSkip 0x%x\n", m68k_read_memory_32(6));
    unsigned int rt_Flags = m68k_read_memory_8(10);
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
    fprintf(stderr, "rt_Version 0x%x\n", m68k_read_memory_8(11));
    fprintf(stderr, "rt_Type 0x%x\n", m68k_read_memory_8(12));
    fprintf(stderr, "rt_Pri 0x%x\n", m68k_read_memory_8(13));
    unsigned int rt_Name = m68k_read_memory_32(14);
    fprintf(stderr, "rt_Name 0x%x '%s'\n", rt_Name, _ram+rt_Name);
    unsigned int rt_IdString = m68k_read_memory_32(18);
    fprintf(stderr, "rt_IdString 0x%x '%s'\n", rt_IdString, _ram+rt_IdString);
    unsigned int rt_Init = m68k_read_memory_32(22);
    fprintf(stderr, "rt_Init 0x%x\n", rt_Init);

    unsigned int translatefunc = 0;
    if (rtf_AutoInit) {
        unsigned int dataSize = m68k_read_memory_32(rt_Init);
        unsigned int vectors = m68k_read_memory_32(rt_Init+4);
        unsigned int structure = m68k_read_memory_32(rt_Init+8);
        unsigned int initFunction = m68k_read_memory_32(rt_Init+12);
        fprintf(stderr, "rtf_AutoInit dataSize 0x%x\n", dataSize);
        fprintf(stderr, "rtf_AutoInit vectors 0x%x\n", vectors);
        fprintf(stderr, "rtf_AutoInit structure 0x%x\n", structure);
        fprintf(stderr, "rtf_AutoInit initFunction 0x%x\n", initFunction);
        unsigned int vector = m68k_read_memory_16(vectors);
        if (vector == 0xffff) {
            for (int i=0;; i++) {
                vector = m68k_read_memory_16(vectors+2+i*2);
                if (vector == 0xffff) {
                    break;
                }
                fprintf(stderr, "vectors i %d vector 0x%x\n", i, vector);
                vector += vectors;
                fprintf(stderr, "vectors i %d -> vector 0x%x\n", i, vector);
                if (i == 4) {
                    fprintf(stderr, "vectors i %d is Translate function\n", i);
                    translatefunc = vector;
                }
            }
        }
    } else {
        fprintf(stderr, "no RTF_AUTOINIT flag, currently unimplemented, will not work\n");
    }

    fprintf(stderr, "translatefunc %x\n", translatefunc);

    int len = strlen(str);
    if (len >= INPUT_BUFSIZE) {
        len = INPUT_BUFSIZE;
    }
    strncpy(_ram + _inputbase, str, len);
    m68k_set_reg(M68K_REG_A0, _inputbase);
    m68k_set_reg(M68K_REG_D0, len);
    m68k_set_reg(M68K_REG_A1, _outputbase);
    m68k_set_reg(M68K_REG_D1, OUTPUT_BUFSIZE);
    m68k_set_reg(M68K_REG_SP, _stackpointer);

    m68k_set_reg(M68K_REG_A6, _librarybase);

    m68k_write_memory_16(_mainbase, 0x4eb9); //jsr
    m68k_write_memory_32(_mainbase+2, translatefunc);
    m68k_write_memory_32(_mainbase+6, 0x4e722700); //stop $2700

    m68k_set_reg(M68K_REG_PC, _mainbase);
}

void process_library(char *str)
{
    if ((_ram[0] == 0x4a) && (_ram[1] == 0xfc)) {
        fprintf(stderr, "ROMTag found\n");
        process_library_with_romtag(str);
        return;
    }

    fprintf(stderr, "no ROMTag\n");

    int len = strlen(str);
    if (len >= INPUT_BUFSIZE) {
        len = INPUT_BUFSIZE;
    }
    strncpy(_ram + _inputbase, str, len);
    m68k_set_reg(M68K_REG_A0, _inputbase);
    m68k_set_reg(M68K_REG_D0, len);
    m68k_set_reg(M68K_REG_A1, _outputbase);
    m68k_set_reg(M68K_REG_D1, OUTPUT_BUFSIZE);
    m68k_set_reg(M68K_REG_SP, _stackpointer);

    m68k_set_reg(M68K_REG_A6, _librarybase);

    m68k_write_memory_16(_mainbase, 0x4eb9); //jsr
    m68k_write_memory_32(_mainbase+2, 0x134); //Translate, hardcoded, should get from MakeLibrary
    m68k_write_memory_32(_mainbase+6, 0x4e722700); //stop $2700

    m68k_set_reg(M68K_REG_PC, _mainbase);
}

unsigned int m68k_read_memory_8(unsigned int addr)
{
    unsigned int val = _ram[addr];
//fprintf(stderr, "m68k_read_memory_8 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_memory_16(unsigned int addr)
{
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p;
//fprintf(stderr, "m68k_read_memory_16 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_memory_32(unsigned int addr)
{
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p;
//fprintf(stderr, "m68k_read_memory_32 %x %x\n", addr, val);
    return val;
}

void m68k_write_memory_8(unsigned int addr, unsigned int val)
{
fprintf(stderr, "m68k_write_memory_8 addr %x val %x\n", addr, val);
    _ram[addr] = val;
}

void m68k_write_memory_16(unsigned int addr, unsigned int val)
{
fprintf(stderr, "m68k_write_memory_16 addr %x val %x\n", addr, val);
    uint8_t *p = (uint8_t *) &_ram[addr];
    p[1] = val&0xff;
    val >>= 8;
    p[0] = val&0xff;
}

void m68k_write_memory_32(unsigned int addr, unsigned int val)
{
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

unsigned int m68k_read_disassembler_8(unsigned int addr)
{
    unsigned int val = _ram[addr];
//fprintf(stderr, "m68k_read_disassembler_8 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_disassembler_16(unsigned int addr)
{
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p;
//fprintf(stderr, "m68k_read_disassembler_16 %x %x\n", addr, val);
    return val;
}

unsigned int m68k_read_disassembler_32(unsigned int addr)
{
    uint8_t *p = (uint8_t *) &_ram[addr];
    unsigned int val;
    val = *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p++;
    val <<= 8;
    val |= *p;
//fprintf(stderr, "m68k_read_disassembler_32 %x %x\n", addr, val);
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
	fprintf(stderr, "Execute %03x: %-20s: %s (SP %x)\n", pc, buf2, buf, sp);
    unsigned int instr = m68k_read_memory_16(pc);
    if (instr == 0x4e72) {
        fprintf(stderr, "***** Stop\n");
        printf("%s\n", _ram+_outputbase);
        exit(0);
    }
}

void main(int argc, char **argv)
{
    unsigned char *text = 0;
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "-l")) {
            if (i+1 < argc) {
                _library_path = argv[i+1];
                i++;
            } else {
                fprintf(stderr, "error, expecting path for -l\n");
                exit(1);
            }
        } else {
            text = argv[i];
        }
    }

    if (!text) {
        fprintf(stderr, "Usage: %s [-l translator_library_file] <text>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "%s \"Hello world.\"\n", argv[0]);
        fprintf(stderr, "%s -l translator.library~1.2 \"Hello world.\"\n", argv[0]);
        fprintf(stderr, "%s -l translator.library~1.3.3 \"Hello world.\"\n", argv[0]);
        fprintf(stderr, "%s -l translator.library~2.04 \"Hello world.\"\n", argv[0]);
        exit(1);
    }

    for (int i=0; i<MAX_RAM; i++) {
        _ram[i] = 0;
    }
    load_library();
    copy_library_to_ram();

    m68k_init();
    m68k_set_instr_hook_callback(instr_hook_callback);
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();

    process_library(text);
    for(;;) {
        m68k_execute(100000);
    }

    exit(0);
}


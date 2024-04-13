#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#define MAX_MEMORY (1 << 16) // 16-bit 64KiB memory
uint16_t memory[MAX_MEMORY];

enum {
	R_R0 = 0,
	R_R1,
	R_R2,
	R_R3,
	R_R4,
	R_R5,
	R_R6,
	R_R7,
	R_PC,
	R_COND,
	R_COUNT
};

uint16_t registers[R_COUNT];

enum {
	OP_BR = 0, // branch
	OP_ADD,    // add
	OP_LD,     // load
	OP_ST,     // store
	OP_JSR,    // jump register
	OP_AND,    // bitwise and
	OP_LDR,    // load register
	OP_STR,    // store register
	OP_RTI,    // unused
	OP_NOT,    // bitwise not
	OP_LDI,    // load indirect
	OP_STI,    // store indirect
	OP_JMP,    // jump
	OP_RES,    // reserved (unused)
	OP_LEA,    // load effective address
	OP_TRAP    // execute trap
};

enum {
	FL_POS = 1 << 0, // positive  -> 1
	FL_ZRO = 1 << 1, // zero      -> 2
	FL_NEG = 1 << 2  // negative  -> 4
};

enum {
	TRAP_GETC  = 0X20,
	TRAP_OUT   = 0X21,
	TRAP_PUTS  = 0X22,
	TRAP_IN    = 0X23,
	TRAP_PUTSP = 0X24,
	TRAP_HALT  = 0X25
};

enum
{
    MR_KBSR = 0xFE00, // keyboard status 
    MR_KBDR = 0xFE02  // keyboard data 
};

struct termios original_tio;

void debug() {
	for(int i = 0; i < 8; i++) {
		printf("R_%d: %04x\n", i, registers[i]);
	}
	printf("R_PC: %04x\n", registers[R_PC]);
	uint16_t val = registers[R_COND];
	printf("R_COND: N: %d, Z: %d, P: %d\n", (val >> 2) & 0x1, (val >> 1) & 0x1, (val >> 0) & 0x1);
	printf("\n");
}

uint16_t sign_extend(uint16_t x, int bit_count) {
	if(x >> (bit_count - 1) & 1) {
		x |= (0xffff << bit_count);
	}
	return x;
}

void disable_input_buffering() {
	tcgetattr(STDIN_FILENO, &original_tio);
	struct termios new_tio = original_tio;
	new_tio.c_lflag &= ~ICANON & ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
	tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key() {
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

uint16_t memory_read(uint16_t address) {
	if (address == MR_KBSR) {
		if (check_key()) {
			memory[MR_KBSR] = (1 << 15);
			memory[MR_KBDR] = getchar();
		} else {
			memory[MR_KBSR] = 0;
		}
	}
	return memory[address];
}

void memory_write(uint16_t address, uint16_t word) {
	memory[address] = word;
}

void update_flags(uint16_t reg) {
	if (registers[reg] == 0) {
		registers[R_COND] = FL_ZRO;
	} else if (registers[reg] >> 15) {
		registers[R_COND] = FL_NEG;
	} else {
		registers[R_COND] = FL_POS;
	}
}

uint16_t swap(uint16_t x) {
	return (x << 8) | (x >> 8);
}

void read_image_file(FILE *file) {
	uint16_t origin;
	fread(&origin, sizeof(origin), 1, file);
	origin = swap(origin);

	uint16_t max_read = MAX_MEMORY - origin;
	uint16_t *p = memory + origin;
	size_t read = fread(p, sizeof(uint16_t), max_read, file);

	while(read-- > 0) {
		*p = swap(*p);
		++p;
	}
}

int read_image(const char *file_path) {
	FILE *file = fopen(file_path, "rb");
	if(!file) { return 0; }
	read_image_file(file);
	return 1;
}

void handle_interrupt(int signal) {
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

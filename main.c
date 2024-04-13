// https://www.jmeiners.com/lc3-vm/

#include "lc3.c"

int main(int argc, char **argv) {

	// Load Arguments
	if(argc < 2) {
		printf("lc3 [image-file1] ..\n");
		exit(2);
	}

	for(int j = 1; j < argc; ++j) {
		if(!read_image(argv[j])) {
			fprintf(stderr, "Failed to load image: %s\n", argv[j]);
			exit(1);
		}
	}

	// Setup
	signal(SIGINT, handle_interrupt);
	disable_input_buffering();
	registers[R_COND] = FL_ZRO; // one flag set at a time

	// PC = 0x3000 (default)
	enum { PC_START = 0x3000 };
	registers[R_PC] = PC_START;

	int running = 1;
	debug();
	while (running) {
		// Fetch
		uint16_t instr = memory_read(registers[R_PC]++); 
		uint16_t opcode = instr >> 12;
		uint16_t dr, sr1, sr2, imm5, address, *b; int imm_flag; char c;

		// Decode
		switch (opcode) {
			case OP_ADD:
				dr = (instr >> 9) & 0x7;
				sr1 = (instr >> 6) & 0x7;
				imm_flag = (instr >> 5) & 0x1;

				if(imm_flag) {
					imm5 = sign_extend(instr & 0x1f, 5);
					registers[dr] = registers[sr1] + imm5;
				} else {
					sr2 = instr & 0x7;
					registers[dr] = registers[sr1] + registers[sr2];
				}
				update_flags(dr);
				break;

			case OP_AND:
				dr = (instr >> 9) & 0x7;
				sr1 = (instr >> 6) & 0x7;
				imm_flag = (instr >> 5) & 0x1;

				if(imm_flag) {
					imm5 = sign_extend(instr & 0x1f, 5);
					registers[dr] = registers[sr1] & imm5;
				} else {
					sr2 = instr & 0x7;
					registers[dr] = registers[sr1] & registers[sr2];
				}
				update_flags(dr);
				break;

			case OP_NOT:
				dr = (instr >> 9) & 0x7;
				sr1 = (instr >> 6) & 0x7;
				registers[dr] = ~registers[sr1];
				update_flags(dr);
				break;

			case OP_BR:
				if((registers[R_COND] == 4 && (instr & 0x0800) >> 11) || (registers[R_COND] == 2 && 
				(instr & 0x0400) >> 10) || (registers[R_COND] == 1 && (instr & 0x0200) >> 9)) {
					registers[R_PC] += sign_extend(instr & 0x01ff, 9);
				}	
				break;

			case OP_JMP:
				registers[R_PC] = registers[(instr >> 6) & 0x7];
				break;

			case OP_JSR:
				registers[R_R7] = registers[R_PC];
				if((instr >> 11) & 0x1) {
					registers[R_PC] += sign_extend(instr & 0x07ff, 11);
				} else {
					registers[R_PC] = registers[(instr >> 6) & 0x7];
				}
				break;

			case OP_LD:
				dr = (instr >> 9) & 0x7;
				registers[dr] = memory_read(registers[R_PC] + sign_extend(instr & 0x01ff, 9));
				update_flags(dr);
				break;

			case OP_LDI:
				dr = (instr >> 9) & 0x7;
				registers[dr] = memory_read(memory_read(registers[R_PC] + sign_extend(instr & 0x01ff, 9)));
				update_flags(dr);
				break;

			case OP_LDR:
				dr = (instr >> 9) & 0x7;  //   |
				registers[dr] = memory_read(registers[(instr >> 6) & 0x7] + sign_extend(instr & 0x003f, 6));
				update_flags(dr);
				break;

			case OP_LEA:
				dr = (instr >> 9) & 0x7;
				registers[dr] = registers[R_PC] + sign_extend(instr & 0x01ff, 9);
				update_flags(dr);
				break;

			case OP_ST:
				sr1 = (instr >> 9) & 0x7;
				memory_write(registers[R_PC] + sign_extend(instr & 0x01ff, 9), registers[sr1]);
				break;

			case OP_STI:
				sr1 = (instr >> 9) & 0x7;
				memory_write(memory_read(registers[R_PC] + sign_extend(instr & 0x01ff, 9)), registers[sr1]);
				break;

			case OP_STR:
				sr1 = (instr >> 9) & 0x7;
				memory_write(registers[(instr >> 6) & 0x7] + sign_extend(instr & 0x003f, 6), registers[sr1]);
				break;

			case OP_TRAP: 
				registers[R_R7] = registers[R_PC];
				switch(instr & 0xff) {
					case TRAP_GETC:
						registers[R_R0] = (uint16_t)getchar();
						update_flags(R_R0);
						break;

					case TRAP_OUT:
						putc((char)registers[R_R0], stdout);
						fflush(stdout);
						break;

					case TRAP_PUTS:
						b = memory + registers[R_R0];
						while(*b) {
							putc((char)*b, stdout);
							++b;
						}
						fflush(stdout);
						break;

					case TRAP_IN:
						fprintf(stdout, "Enter a character:\n");
						c = getchar();
						putc(c, stdout);
						fflush(stdout);
						registers[R_R0] = (uint16_t) c;
						update_flags(R_R0);
						break;

					case TRAP_PUTSP:
						b = memory + registers[R_R0];
						while(*b) {
							putc((*b) & 0xff, stdout);
							if((*b) >> 8) putc((*b) >> 8, stdout);
							++b;
						}
						fflush(stdout);
						break;

					case TRAP_HALT:
						puts("HALT");
						fflush(stdout);
						running = 0;
						break;
				}
				break;
			case OP_RES:
			case OP_RTI:
			default:
				// bad opcode
				break;
		}
		
		// printf("Instr: %04x\n", instr);
		// debug();
	}
	
	restore_input_buffering();

	return 0;
}

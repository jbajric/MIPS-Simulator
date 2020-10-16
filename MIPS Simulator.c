#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define MIPS_REGS 32 // broj registara
#define MEM_DATA_START 0x00000000 // početak dijela memorije za podatke
#define MEM_DATA_SIZE 0x00000100 // veličina dijela memorije za podatke
#define MEM_INSTRUCTIONS_START 0x00000100 // početak dijela memorije za instrukcije
#define MEM_INSTRUCTIONS_SIZE 0x00000080 // veličina dijela memorije za instrukcije

// stanje procesora
typedef struct CPU_State {
  uint32_t PC; // programski brojac
  uint32_t REGS[MIPS_REGS]; // register file
} CPU_State;


// regioni glavne memroije (dio za podatke, dio za instrukcije)
typedef struct {
    uint32_t start, size; // početak regiona i veličina
    uint8_t *mem; // sadržaj
} mem_region;

mem_region MEM_REGIONS[] = {
    {MEM_DATA_START, MEM_DATA_SIZE, NULL},
    {MEM_INSTRUCTIONS_START, MEM_INSTRUCTIONS_SIZE, NULL}
};

#define MEM_NREGIONS 2

typedef struct  {
    enum {ADD, SUB, SW, LW, OR} opc; 
    uint32_t  rd,rt,rs,im;  
} CUR_inst;

CUR_inst instrukcija;

// trenutno stanje CPU-a
CPU_State CURRENT_STATE, NEXT_STATE, LAST_STATE;


// funkcija koja piše u glavnu memoriju
void mem_write(uint32_t address, uint32_t value) {
    int i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        if (address >= MEM_REGIONS[i].start &&
                address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
            uint32_t offset = address - MEM_REGIONS[i].start;
            // memorija je byte-adresabilna, pa se jedna (32-bitna) riječ "dijeli" na 4 dijela
            MEM_REGIONS[i].mem[offset+3] = (value >> 24) & 0xFF;
            MEM_REGIONS[i].mem[offset+2] = (value >> 16) & 0xFF;
            MEM_REGIONS[i].mem[offset+1] = (value >>  8) & 0xFF;
            MEM_REGIONS[i].mem[offset+0] = (value >>  0) & 0xFF;
            return;
        }
    }
}

// memorija koja čita vrijednost iz glavne memorije
uint32_t mem_read(uint32_t address) {
    int i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        if (address >= MEM_REGIONS[i].start &&
                address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
            uint32_t offset = address - MEM_REGIONS[i].start;
            // memorija je byte-adresabilna, pa pri čitanju 32-bitne riječi čitaju se 4 byte-a
            return
                (MEM_REGIONS[i].mem[offset+3] << 24) |
                (MEM_REGIONS[i].mem[offset+2] << 16) |
                (MEM_REGIONS[i].mem[offset+1] <<  8) |
                (MEM_REGIONS[i].mem[offset+0] <<  0);
        }
    }

    return 0;
}

// inicijalizacija glavne memorije
void init_memory() {
    int i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        MEM_REGIONS[i].mem = malloc(MEM_REGIONS[i].size);
        memset(MEM_REGIONS[i].mem, 0, MEM_REGIONS[i].size);
    }
}

// funkcija koja učitava program iz .txt datoteke u memoriju instrukcija
void load_program(char *program_filename) {
	FILE *input = fopen(program_filename, "r");
	uint32_t counter = MEM_INSTRUCTIONS_START;
	CURRENT_STATE.PC = counter;
	uint32_t results;
	while (fscanf(input, "%x\n", &results) == 1) {
		mem_write(counter, results);
		counter += 0x4;
	}
	fclose (input);
}

// funkcija koja dekodira instrukciju
void decode_instruction(uint32_t instruction) {
    uint32_t opcode =(instruction >> 26) & 0x3F;
    int rs = (instruction >> 21) & 0x1F;

    //Provjera formata instrukcije (r ili i)
    if (opcode == 0) {
        int funct = instruction & 0x3F;
        int rt = (instruction >> 16) & 0x1F;
        int rd = (instruction >> 11) & 0x1F;
        instrukcija.opc = -1;
        if ( rt < 8 || rt > 23 || rd < 8 || rd > 23  || rs < 8 || rs > 23 ) {
            printf ("Not defined\n");
        }
        if (funct == 32) {
            printf("ADD");
            instrukcija.opc = ADD;
        }
        else if (funct == 34) {
            printf ("SUB");
            instrukcija.opc = SUB;
        }
        else if (funct == 37) {
            printf("OR");
            instrukcija.opc = OR;
        }
        else {
            printf ("Not defined\n");
            return;
        }
        if (rt >= 8 && rt <= 15) 
            printf (" $t%d, ", (instrukcija.rt = rt) - 8);
        else   
            printf ("$s%d,", (instrukcija.rt = rt) - 16);
        if (rs >= 8 && rs <= 15) 
            printf (" $t%d,", (instrukcija.rs = rs) - 8);
        else if (rs == 0) 
            printf (" $0,");
        else if (rs >= 16 && rs <= 23) 
            printf (" $s%d,", (instrukcija.rs = rs) - 16);
        if (rd >= 8 && rd <= 15) 
            printf (" $t%d", (instrukcija.rd = rd) - 8);
        else if (rd == 0)  
            printf ("$0");
        else   
            printf (" $s%d",  (instrukcija.rd = rd) - 16);

    } 
    else {
        //I tip 
        short int immediate = instruction & 0xFFFF;
        instrukcija.im = immediate;
        int rt = (instruction >> 16) & 0x1F;
        if (opcode == 35) {
            printf ("lw ");
            instrukcija.opc = LW;
        } 
        else if (opcode == 43) {
            printf ("sw ");
            instrukcija.opc = SW;
        } 
        else {
            printf("Not defined\n");
            return;
        }
        if (rt >= 8 && rt <= 15) 
            printf (" $t%d,", (instrukcija.rt = rt) - 8);
        else if (rt == 0)  
            printf (" $0,");
        else   
            printf (" $s%d,", (instrukcija.rt = rt) - 16);
        if (rs>=8 && rs<=15) 
            printf ("%d($t%d)", immediate, (instrukcija.rs = rs) - 8);
        else if (rs != 0)  
            printf ("%d($s%d)", immediate, (instrukcija.rs = rs) - 16);
        else 
            printf (" $0");
    }
    printf ("\n");
}

void execute_instruction (uint32_t instruction) {
    decode_instruction(instruction);
    if (instrukcija.opc == ADD) {
        NEXT_STATE.REGS[instrukcija.rd] =
                CURRENT_STATE.REGS[instrukcija.rd] + (CURRENT_STATE.REGS[instrukcija.rs] + CURRENT_STATE.REGS[instrukcija.rt]);
        CURRENT_STATE.REGS[instrukcija.rd] = NEXT_STATE.REGS[instrukcija.rd]; 
    } 
    else if (instrukcija.opc == SUB) {
        NEXT_STATE.REGS[instrukcija.rd] =
                CURRENT_STATE.REGS[instrukcija.rd] - (CURRENT_STATE.REGS[instrukcija.rs] + CURRENT_STATE.REGS[instrukcija.rt]);
        CURRENT_STATE.REGS[instrukcija.rd] = NEXT_STATE.REGS[instrukcija.rd];
    } 
    else if (instrukcija.opc == SW) {
        mem_write( CURRENT_STATE.REGS[instrukcija.rs] + instrukcija.im, CURRENT_STATE.REGS[instrukcija.rt]);
    } 
    else if (instrukcija.opc == LW) {
        NEXT_STATE.REGS[instrukcija.rt] = mem_read(NEXT_STATE.REGS[instrukcija.rs] + instrukcija.im);
        CURRENT_STATE.REGS[instrukcija.rs] = NEXT_STATE.REGS[instrukcija.rs];
    } 
    else if (instrukcija.opc == OR) {
        NEXT_STATE.REGS[instrukcija.rd] = (CURRENT_STATE.REGS[instrukcija.rs] | CURRENT_STATE.REGS[instrukcija.rt]); //OR bit po bit
        CURRENT_STATE.REGS[instrukcija.rd] = NEXT_STATE.REGS[instrukcija.rd]; 
    }
    else 
        printf("Not defined\n");
    
    instrukcija.im = -1;
    instrukcija.opc = -1;
    instrukcija.rs = -1;
    instrukcija.rd = -1;
    instrukcija.rt = -1;
}

void stanjeReg() {
    for (int i = 0 ; i < 32; i++ )
        printf ("REGISTAR%d: %x \n", i, CURRENT_STATE.REGS[i]);
}

//Punimo registre random vrijednostima
void reg_value_init() {
    time_t t;
    srand(time(&t));
    for (int i = 0 ; i < 32; i++)
        CURRENT_STATE.REGS[i] = 1 + rand()%50;
}

void start() {
    time_t t;
    srand( time( &t ) );
    for (int i = 0; i < MEM_DATA_START + MEM_DATA_SIZE; i += 4) {
        uint32_t w = 1 + rand()%1000;
        mem_write (MEM_DATA_START + i, w);
    }
}


void read() {
    for (int i = 0 ; i < MEM_DATA_START + MEM_DATA_SIZE; i += 4) 
        printf( "%d: %x ", i/4, mem_read (MEM_DATA_START + i));
    printf ("\n");
}



int main() {
    init_memory();
    load_program("program.txt");
    printf ("Vrijednosti registara: \n");
    reg_value_init();
    stanjeReg();
    printf ("Vrijednosti mjesta podataka: \n");
    start();
    read();
    uint32_t p = MEM_INSTRUCTIONS_START;
    printf ("Zavrsene instrukcije su: \n");
    do {
        execute_instruction(mem_read(p));
        p += 4;
    } 
    while (p < LAST_STATE.PC);
    printf ("Finalne vrijednosti registara su: \n");
    stanjeReg();
    printf ("Finalno stanje mjesta podataka: \n");
    read();
    return 0;
}
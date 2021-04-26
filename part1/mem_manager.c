#include <stdio.h>
#include <stdlib.h>

struct PTE {
    int frame_no;
    int valid;
    int dirty;
};

const int NUM_PAGES = 256; //2^8 entries in page table
const int PAGE_SIZE = 256;

int update_page_table(int logical_pg, struct PTE* page_table,
                      char* physical_mem, FILE* bfp,
                      int next_free_frame) {
    //translate page to frame
    if(!page_table[logical_pg].valid) {
        //page fault
        //read in logical_pg from BACKING STORE and store in a
        //pg frame in physical memory
        //DEBUG CODE BELOW
        //int read_at = logical_pg * PAGE_SIZE;
        //int read_to = next_free_frame * PAGE_SIZE;
        //printf("Reading from page %d(%d) to frame %d(%d)\n",
        //       logical_pg, read_at, next_free_frame, read_to);
        fseek(bfp, logical_pg * PAGE_SIZE, SEEK_SET);
        fread(physical_mem + next_free_frame * PAGE_SIZE, PAGE_SIZE,
              1, bfp);
        page_table[logical_pg].frame_no = next_free_frame++;
        page_table[logical_pg].valid = 1;
    }
    return next_free_frame;
}

int main (int argc, char** argv) {
 
    //page table using PTE as the data type
    //struct PTE pageTable[SIZE];
    
    
    //default filename
    char* addresses_fname = "addresses.txt";
    char* back_store_fname = "BACKING_STORE.bin";
    
    //set filename
    if(argc >= 2) {
        addresses_fname = argv[1];
    }
    if(argc == 3) {
        back_store_fname = argv[2];
    }
    
    //create fp and open file, perform error checking
    FILE * afp = fopen(addresses_fname,"r");
    if(!afp) {
        fprintf(stderr, "Address file failed to open! Exiting program...\n");
        exit(-1);
    }
    
    FILE * bfp = fopen(back_store_fname, "rb");
    if(!bfp) {
        fprintf(stderr, "Binary store file failed to open! Exiting program...\n");
        exit(-1);
    }
    
    
    //create page table
    struct PTE* page_table = (struct PTE *) calloc(NUM_PAGES, sizeof(struct PTE));
    //create physical memory
    char* physical_mem = (char *) malloc(NUM_PAGES * PAGE_SIZE);
    int next_free_frame = 0;
    
    //read integers from file
    int entry;
    fscanf(afp, "%d", &entry);
    while (!feof(afp)) {
        //extract offset and page number
        //255 = 0xFF ends in 11111111 in binary
        int logical_addr = entry & 0xFFFF;
        int write_bit = (entry >> 16) & 0x1;
        
        int offset = logical_addr & 0xFF;
        int logical_pg = (logical_addr >> 8) & 0xFF;
        
        next_free_frame = update_page_table(logical_pg, page_table,
                                            physical_mem, bfp,
                                            next_free_frame);
        int physical_addr =
            page_table[logical_pg].frame_no * PAGE_SIZE + offset;
        if(write_bit) {
            // page is dirty
            physical_mem[physical_addr]++;
            page_table[logical_pg].dirty = 1;
        }
        
        printf("0x%04X 0x%04X %d %d\n",
               logical_addr,
               physical_addr,
               (int) physical_mem[physical_addr],
               page_table[logical_pg].dirty);
        
        fscanf(afp, "%d", &entry);
        //fflush(stdout);
    }
    
	fclose(afp);
    fclose(bfp);
    
	return 0;
}

#include <stdio.h>
#include <stdlib.h>

struct PTE {
    int frame_no;
    int valid;
    int dirty;
};

struct TLBE {
    int page_no;
    struct PTE pte;
};

struct PageTable {
    struct PTE * table;
    int next_free_frame;
    int num_faults;
};

// FifoTlb
struct TlbFifo {
    struct TLBE * tlb;
    int first;
    int size;
    int num_hits;
};

const int NUM_PAGES = 256; //2^8 entries in page table
const int PAGE_SIZE = 256;
const int NUM_TLB_ENTRIES = 16;

/**
 This function checks TLB in circular fashion and returns index into TLB if page number is found,
 otherwise returns -1.
 parameters:
 struct TlbFife* tlb_table: passing in a pointer to the tlb_table which will be searched for a page #
 int logical_pg: the page to be searched for in the tlb_table
 */
int tlb_find_entry(struct TlbFifo* tlb_table, int logical_pg) {
    //check tlb in circular fashion
    struct TLBE * tlb = tlb_table->tlb;
    for(int i = 0; i < tlb_table->size; i++) {
        int index = (tlb_table->first + i) % NUM_TLB_ENTRIES;
        if(tlb[index].pte.valid && tlb[index].page_no == logical_pg) {
            return index;
        }
    }
    return -1;
}

/**
 This function adds an entry to the TLB table (a circular buffer). Returns the old (pre modified) TLB
 parameters:
 struct TlbFifo* tlb_table: a pointer to the TLB table to which an entry must be added to
 int logical_pg: page number which must be added
 struct PTE pte: page table entry associated with page # which must be added
 */
struct TLBE tlb_add_entry(struct TlbFifo* tlb_table, int logical_pg, struct PTE pte) {
    int index = (tlb_table->first + tlb_table->size) % NUM_TLB_ENTRIES;
    struct TLBE old = tlb_table->tlb[index];
    tlb_table->tlb[index].page_no = logical_pg;
    tlb_table->tlb[index].pte = pte;
    // update circular buffer
    if (tlb_table->size < NUM_TLB_ENTRIES) {
        tlb_table->size++;
    } else {
        tlb_table->first = (tlb_table->first + 1) % NUM_TLB_ENTRIES;
    }
    return old;
}

/**
 Returns the last entry from TLB
 parameters:
 struct TlbFifo* tlb_table: TLB from which last entry must be retrieved using modular arithmetic
 */
struct TLBE* tlb_get_last_entry(struct TlbFifo* tlb_table) {
    int index = (tlb_table->first + tlb_table->size - 1) % NUM_TLB_ENTRIES;
    return &tlb_table->tlb[index];
}

/**
 Gets table entry from either TLB or, if not found in TLB then checks page table. Handles page fault
 if not found in page table. Finally (if it was not found in TLB) updates TLB and returns last entry in
 TLB table (most recent).
 parameters:
 int logical_pg: the logical_pg to be searched
 struct TlbFifo* tlb_table: pointer to TLB which must be searched for logical_pg
 struct PageTable* page_table: page table which is searched in case of TLB miss
 char* physical_mem: physical memory is updated in case of page fault
 FILE* bfp: file pointer to file that will be searched in case of page fault
 */
struct PTE* get_table_entry(int logical_pg,
                           struct TlbFifo* tlb_table,
                           struct PageTable* page_table,
                           char* physical_mem, FILE* bfp) {
    
    int tlb_index = tlb_find_entry(tlb_table, logical_pg);
    if (tlb_index >= 0) {
        // found
        tlb_table->num_hits++;
        return &tlb_table->tlb[tlb_index].pte;
    }
    
    // tlb did not find logical page
    // translate from page table to frame
    if(!page_table->table[logical_pg].valid) {
        //page fault
        //read in logical_pg from BACKING STORE and store in a
        //pg frame in physical memory
        //DEBUG CODE BELOW
        //int read_at = logical_pg * PAGE_SIZE;
        //int read_to = next_free_frame * PAGE_SIZE;
        //printf("Reading from page %d(%d) to frame %d(%d)\n",
        //       logical_pg, read_at, next_free_frame, read_to);
        fseek(bfp, logical_pg * PAGE_SIZE, SEEK_SET);
        fread(physical_mem + page_table->next_free_frame * PAGE_SIZE,
              PAGE_SIZE, 1, bfp);
        page_table->table[logical_pg].frame_no = page_table->next_free_frame++;
        page_table->table[logical_pg].valid = 1;
        page_table->num_faults++;
    }
    
    //update tlb
    struct TLBE old_tlb_entry = tlb_add_entry(tlb_table, logical_pg, page_table->table[logical_pg]);
    // if removed tlb entry is dirty, we update page table
    if (old_tlb_entry.pte.valid && old_tlb_entry.pte.dirty) {
        page_table->table[old_tlb_entry.page_no].dirty = 1;
    }
    
    return &tlb_get_last_entry(tlb_table)->pte;
}

int main (int argc, char** argv) {
    
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
    
    //create TLB
    struct TlbFifo tlb_fifo;
    tlb_fifo.tlb = (struct TLBE *) calloc(NUM_TLB_ENTRIES, sizeof(struct TLBE));
    tlb_fifo.first = 0;
    tlb_fifo.size = 0;
    tlb_fifo.num_hits = 0;

    //create page table
    struct PageTable page_table;
    page_table.table = (struct PTE *) calloc(NUM_PAGES, sizeof(struct PTE));
    page_table.next_free_frame = 0;
    page_table.num_faults = 0;
    //create physical memory
    char* physical_mem = (char *) malloc(NUM_PAGES * PAGE_SIZE);
    
    //read integers from file
    int entry;
    int num_entries = 0;
    int num_dirty = 0;
    fscanf(afp, "%d", &entry);
    while (!feof(afp)) {
        ++num_entries;
        //extract offset and page number
        //255 = 0xFF ends in 11111111 in binary
        int logical_addr = entry & 0xFFFF;
        int write_bit = (entry >> 16) & 0x1;
        
        int offset = logical_addr & 0xFF;
        int logical_pg = (logical_addr >> 8) & 0xFF;
        
        struct PTE* pte = get_table_entry(logical_pg, &tlb_fifo, &page_table, physical_mem, bfp);
        int physical_addr = pte->frame_no * PAGE_SIZE + offset;
        
        if(write_bit) {
            // page is dirty
            physical_mem[physical_addr]++;
            pte->dirty = 1;
        }
        
        printf("0x%04X 0x%04X %d %d\n",
               logical_addr,
               physical_addr,
               (int) physical_mem[physical_addr],
               pte->dirty);
        
        fscanf(afp, "%d", &entry);
    }
    
	fclose(afp);
    fclose(bfp);
    
    printf("Page-fault rate: %f\n", page_table.num_faults / (double)num_entries);
    printf("TLB hit rate: %f\n", tlb_fifo.num_hits / (double)num_entries);
    for(int i = 0; i < NUM_PAGES; ++i) {
        num_dirty += (page_table.table[i].dirty && page_table.table[i].valid);
    }
    printf("Number of dirty pages: %d\n", num_dirty);
    
	return 0;
}

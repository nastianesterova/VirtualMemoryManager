#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
    int next_frame;
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
const int NUM_FRAMES = 128;
const int NUM_TLB_ENTRIES = 16;


/**
 This function reads/writes pages to file
 */
void read_write_page_data(void* memory, int page_num, int is_write, FILE * fh) {
    if (fseek(fh, page_num * PAGE_SIZE, SEEK_SET)) {
        fprintf(stderr, "Cannot seek to %d in %s: %s\n", page_num * PAGE_SIZE, fh, strerror(errno));
        exit(-1);
    }

    if (is_write) {
        // printf("Writing page 0x%04X\n", page_num * PAGE_SIZE);
        int c = fwrite(memory, PAGE_SIZE, 1, fh);
        if (c != 1) {
            fprintf(stderr, "Cannot write to %s: %s\n", fh, strerror(errno));
            exit(-1);
        }
    } else {
        // printf("Reading page 0x%04X\n", page_num * PAGE_SIZE);
        int c = fread(memory, PAGE_SIZE, 1, fh);
        if (c != 1) {
            fprintf(stderr, "Cannot read from %s: %s\n", fh, strerror(errno));
            exit(-1);
        }
    }
}


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
 Returns the last entry from TLB
 parameters:
 struct TlbFifo* tlb_table: TLB from which last entry must be retrieved using modular arithmetic
 */
struct TLBE* tlb_get_last_entry(struct TlbFifo* tlb_table) {
    int index = (tlb_table->first + tlb_table->size - 1) % NUM_TLB_ENTRIES;
    return &tlb_table->tlb[index];
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

void tlb_remove_entry(struct TlbFifo* tlb_table, int logical_pg) {
    struct TLBE * tlb = tlb_table->tlb;
    int swap_index = -1;
    for (int i = 0; i < tlb_table->size; i++) {
        int index = (tlb_table->first + i) % NUM_TLB_ENTRIES;
        if(tlb[index].pte.valid && tlb[index].page_no == logical_pg) {
            swap_index = index;
            //printf("Removing from TLB page 0x%02X index %d tlb size %d\n",
            //       logical_pg, i, tlb_table->size);
            continue;
        }
        if (swap_index >=0) {
            tlb[swap_index] = tlb[index];
            swap_index = index;
        }
    }
    if (swap_index >= 0) {
        // Clear last entry
        struct TLBE * entry = tlb_get_last_entry(tlb_table);
        entry->page_no = 0;
        entry->pte.frame_no = 0;
        entry->pte.valid = 0;
        entry->pte.dirty = 0;

        tlb_table->size--;
    }
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
                           char* physical_mem,
                           FILE* back_store) {
    
    int tlb_index = tlb_find_entry(tlb_table, logical_pg);
    if (tlb_index >= 0) {
        // found
        tlb_table->num_hits++;
        return &tlb_table->tlb[tlb_index].pte;
    }
    
    // tlb did not find logical page
    // translate from page table to frame
    if(!page_table->table[logical_pg].valid) {
        //page fault: the entry in the page table at logical_pg is not valid
        //read in logical_pg from BACKING STORE and store it in physical memory
        //First, need to check if there is a free frame in physical memory
        //If there are no free frames, must perform page replacement, aka:
        //Replace one of the frames in physical memory with logical_pg that is read from the.
        //BACKING STORE. Before replacing it, may need to write the frame in physical mem
        //into the BACKING STORE - that is, if this frame has been modified since its last load.
        //what characteristics does this frame have? The frame number itself and the page #
        //associated with it
        
        //given fact that frame has 256 entries of logical pages
        //and that logical page entry that we want, in page table, is invalid
        
        //go through table and check every frame number
        
        
        //eventually will permanently run out of frames and will always need to replace (once
        //the table is filled)
        if(page_table->num_faults >= 128) { // there are no free frames in physical memory
            //must perform page replacement
            //page_table->next_frame is victim frame. It will be removed from physical memory
            //if write bit == 1, will be written to BACKING STORE
            //now there is a free frame (next_frame)
            
            //go through table and check every frame_no until you find page associated with frame_mo
            for(int i = 0; i < NUM_PAGES; i++) {
                if(page_table->table[i].frame_no == page_table->next_frame &&
                   page_table->table[i].valid) { //found associate page #
                    if(page_table->table[i].dirty) {
                        //will need to write the frame to the BACKING STORE
                        read_write_page_data(physical_mem + page_table->next_frame * PAGE_SIZE,
                                             i, 1, back_store);
                        page_table->table[i].dirty = 0;
                    }
                    //remove page_table->next_frame from physical memory
                    page_table->table[i].valid = 0;
                    page_table->table[i].frame_no = -1;
                    // remove logical page from tlb
                    tlb_remove_entry(tlb_table, i);
                    break;
                }
            }
        }

        // We have a free frame to load page to
        read_write_page_data(physical_mem + page_table->next_frame * PAGE_SIZE,
                             logical_pg, 0, back_store);
        page_table->table[logical_pg].frame_no = page_table->next_frame;
        page_table->table[logical_pg].valid = 1;
        page_table->table[logical_pg].dirty = 0;
        page_table->num_faults++;
        page_table->next_frame = (page_table->next_frame + 1) % NUM_FRAMES;
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
    
    //create fp and open file, perform error checking
    FILE * bfp = fopen(back_store_fname,"r+");
    if(!bfp) {
        fprintf(stderr, "Back store file failed to open! Exiting program...\n");
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
    page_table.next_frame = 0;
    page_table.num_faults = 0;
    //create physical memory
    char* physical_mem = (char *) malloc(NUM_FRAMES * PAGE_SIZE); //TODO: physical mem is half of virtual
    //char* head = physical_mem;
    //char* tail = physical_mem + ((NUM_FRAMES-1) * PAGE_SIZE);
    //printf("NEW PHYS_MEM_SIZE: %d\n", (NUM_FRAMES * PAGE_SIZE));
    
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
        int physical_addr = (pte->frame_no * PAGE_SIZE + offset) % (NUM_FRAMES * PAGE_SIZE);//TODO: physical_addr can only reach half
        //printf("Physical address: %d\n", physical_addr);
        //TODO: need to use % above to loop back around in physical address
        
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
        num_dirty += page_table.table[i].valid && page_table.table[i].dirty;
    }
    printf("Number of dirty pages: %d\n", num_dirty);
    
	return 0;
}

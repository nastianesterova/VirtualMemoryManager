#include <stdio.h>
#include <stdlib.h>

struct PTE {
    int frame_no;
    int valid;
};


int main (int argc, char** argv) {
 
    //page table using PTE as the data type
    int SIZE = 256; //2^8 entries in page table
    struct PTE pageTable[SIZE];
    
    
    //default filename
    char* filename = "practice.txt";
    
    //set filename
    if(argc == 2) {
        filename = argv[1];
    }
    
    //create fp and open file, perform error checking
    FILE * fp;
    int i;
    fp = fopen(filename,"r");
    if(!fp) {
        fprintf(stderr, "File failed to open! Exiting program...\n");
        exit(-1);
    }
    
    //read integers from file
    fscanf(fp, "%d", &i);
    while (!feof(fp)) {
        printf("%d 0x%X\n", i, i);
    
        //extract offset and page number
        //255 = 0xFF ends in 11111111 in binary
        int offset = i & 0xFF;
        int pg = (i >> 8) & 0xFF;
        int write_bit = (i >> 9) & 0x1;
        printf("offset: %d\n", offset);
        printf("page #: %d\n", pg);
        printf("write bit: %d\n\n", write_bit);
        
        fscanf(fp, "%d", &i);
        //fflush(stdout);
    }
    
	fclose(fp);
    
	return 0;
}

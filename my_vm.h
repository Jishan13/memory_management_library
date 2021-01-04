#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of your memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024 //4GB

#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;
//Struct for page table
typedef struct page_table{
    //int index;
    void * phyAddr;
}page_table;

// Represents a page directory entry
typedef page_table** pde_t;

#define TLB_SIZE 120

//Struct for a node in page dir
struct dir_node{
    int index;
    struct page_table* table;
};

//Create a page table dir (An array that indexs page tables)
struct dir_node* page_table_dir;

struct tlb_node{
    time_t timestamp;
    unsigned long vp;
    void* va;
    void* pa;
    int index;
    struct tlb_node* next;
};
//Structure to represents TLB
struct tlb {
    struct tlb_node* head;
    //Assume your TLB is a direct mapped TLB of TBL_SIZE (entries)
    // You must also define wth TBL_SIZE in this file.
    //Assume each bucket to be 4 bytes
};
struct tlb* tlb_store;

//Create bitmaps for Physical and Virtual page table
unsigned char* v_bitmap;
unsigned char* p_bitmap;
void **address;

void SetPhysicalMem();
void replace(void* va, void* pa, unsigned long vpn);
void* Translate(pde_t *pgdir, void *va);
int PageMap(pde_t *pgdir, void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *myalloc(unsigned int num_bytes);
int myfree(void *va, int size);
void PutVal(void *va, void *val, int size);
void GetVal(void *va, void *val, int size);
void MatMult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
int power(int x, int y);

#endif

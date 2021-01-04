#include "my_vm.h"

int offset;
int init = 1;
int external_bits;
int internal_bits;
int tlb_count = 0;
int hit = 0;
double miss = 0;
double translations = 0;
pde_t page_dir;
void * PhyMeM;
unsigned long vp;
unsigned long pp;
pthread_mutex_t Pbitmap;
pthread_mutex_t Vbitmap;
pthread_mutex_t PageDir;
pthread_mutex_t setMemLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tlbLock;


int getExternalBits(void * va){
    unsigned long intAddr = (unsigned long)va;
    return intAddr >> (offset+internal_bits);

}

int getInternalBits(void *va){
    unsigned long intAddr = (unsigned long)va;
    unsigned long Mask = (1<<internal_bits) -1;
    return (intAddr >> offset) & (Mask);

}

int getOffset(void *va){
    unsigned long intAddr = (unsigned long)va;
    unsigned long Mask = (1<<offset) -1;
    return (intAddr & Mask);

}

void* indexToAddress(int index){//Converts given index to virtual address
    void* start = (void*)PGSIZE;
    void* va = (PGSIZE*index)+start;//Calculate virtual address
    return va;
}

int AddressToIndex(void *va){ //given virtual address, find the index
    void* start =(void*)PGSIZE;
    int index = (va-start)/PGSIZE;
    return index;

}
void* indexToPhyAddress(int index){//Converts given index to physical address
    void* start = PhyMeM;
    void* pa = (PGSIZE*index)+start;//Calculate physical address
    return pa;
}
int PhyAddressToIndex(void *pa){
    void* start = PhyMeM;
    int index =  (pa-start)/PGSIZE;
    return index;

}

int getBytesToCpy(void *currA){
 int idx = AddressToIndex(currA);//1
 int nextAddr = (int)indexToAddress(idx+1);//0x2000
 return nextAddr - (int)currA;//2
}
int power(int x, int y){// Gives x raised to the power of y
    int i,result = 1;

    if(y == 0){
        return 1;
    }

    for(i=0;i<y;i++){
        result *= x;
    }
    return result;
}

int freePhyPages(void *va){
    int PTEIndex = getInternalBits(va);
    int PDEIndex = getExternalBits(va);
    pthread_mutex_lock(&PageDir);
    if(page_dir[PDEIndex] == NULL){
        pthread_mutex_unlock(&PageDir);
        return -1;
    }else if(page_dir[PDEIndex][PTEIndex].phyAddr == NULL){
        pthread_mutex_unlock(&PageDir);
        return -1;
    }else{
        if(page_dir[PDEIndex][PTEIndex].phyAddr == NULL){
            pthread_mutex_unlock(&PageDir);
            return -1;
        }else{
            int index = PhyAddressToIndex(page_dir[PDEIndex][PTEIndex].phyAddr);
            pthread_mutex_lock(&Pbitmap);
            p_bitmap[index]=0;
            pthread_mutex_unlock(&Pbitmap);
            page_dir[PDEIndex][PTEIndex].phyAddr = NULL;
            pthread_mutex_unlock(&PageDir);
            return 0;
        }
    }
    pthread_mutex_unlock(&PageDir);
    return -1;
}

void add_TLB_node(struct tlb_node* node){
    if(tlb_store->head == NULL){
        tlb_store->head = node;
        tlb_store->head->next = NULL;
        return;
    }
    struct tlb_node* temp = tlb_store->head;
    while(temp->next != NULL){
        temp = temp->next;
    }
    node->next = NULL;
    temp->next = node;
}

void replace(void* va, void* pa, unsigned long vpn){
    //  struct tlb_node* node = (struct tlb_node*) malloc(sizeof(struct tlb_node));
    //  time(&node->timestamp);
    struct tlb_node* temp = tlb_store->head;
    struct tlb_node* temp1 = tlb_store->head->next;
    time_t temp_time = temp->timestamp;
    while(temp != NULL){
        if(temp->next != NULL){
            if(temp->timestamp > temp->next->timestamp){
                temp_time = temp->next->timestamp;
            }
        }
        else{
            if(temp->timestamp < temp_time){
                temp_time = temp->timestamp;
            }
        }
        temp = temp->next;
    }

    temp = tlb_store->head;
    while(temp != NULL){
        if(temp->timestamp == temp_time){
            time(&temp->timestamp);
            temp->vp = vpn;
            temp->pa = pa;
            temp->va = va;
            return;
        }
        temp = temp->next;
    }

}

/*
Function responsible for allocating and setting your physical memory
*/
void SetPhysicalMem() {
    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    PhyMeM = (void *)malloc(MEMSIZE * sizeof(void));

    //Malloc or mmap MEMSIZE memory(Physical)

    //Cal number of physical pages and vp (#vp != #pp)
    //#of virtual pages = MAX_MEMSIZE/ size of a single page(PGSIZE)
    vp = MAX_MEMSIZE/PGSIZE;
    pp = MEMSIZE/PGSIZE;
    // # of physical pages = MEMSIZE / PGSIZE

    //Calculate offset bits
    offset = log2(PGSIZE);

    //Calculate external and internal bits
    int temp = 32 - offset;
    internal_bits = (int)(temp/2);
    external_bits = temp - internal_bits;
    int pi,vi;
    //Initialize virtual and physical bitmap
    // -> physical bitmap would be num_phys_pages size
    p_bitmap = (unsigned char*)malloc(pp*(sizeof (unsigned char)));
    pthread_mutex_init(&Pbitmap,NULL);
    for(pi =0;pi<pp;pi++){
        p_bitmap[pi] = 0;
    }
    // -> virtual bitmap would be num_virt_pages size Pages should be contiguous
    v_bitmap = (unsigned char*)malloc(vp*(sizeof (unsigned char)));
    pthread_mutex_init(&Vbitmap,NULL);
    for(vi =0;pi<pp;pi++){
        v_bitmap[vi] = 0;
    }

    //If there is no contiguous memory in VAS then fail the alloc
    //Initialize page dir
    int dir_size = 1 << external_bits;
    int pte_size = 1 << internal_bits;
    page_dir= (page_table**)malloc(dir_size * sizeof(page_table**));
    pthread_mutex_init(&PageDir,NULL);
    int i;
    for(i=0;i<dir_size;i++){
        //page_dir[i] = (page_table*)malloc(pte_size *sizeof(page_table));
        // page_dir[i][1].phyAddr = (void*)8192;
        page_dir[i] = NULL;
    }
    pthread_mutex_init(&tlbLock,NULL);
    tlb_store = (struct tlb*)malloc(sizeof(struct tlb));

    // for(i=0;i<TLB_SIZE; i++){
    //   struct tlb_node* node = (struct tlb_node*)malloc(sizeof(struct tlb_node));
    //   node->index = i;
    //   add_TLB_node(node);
    // }
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void *va, void *pa)
{
    if(tlb_count == TLB_SIZE){
        pthread_mutex_lock(&tlbLock);
         
        replace(va,pa,(AddressToIndex(va)+1));
        pthread_mutex_unlock(&tlbLock);
        return 0;
    }

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    pthread_mutex_lock(&tlbLock);
    struct tlb_node* node = (struct tlb_node*)malloc(sizeof(struct tlb_node));
    node->vp = (AddressToIndex(va)+1);
    node->va = va;
    node->pa = pa;
    time(&node->timestamp);
    node->index = tlb_count;
    tlb_count ++;
    add_TLB_node(node);
    pthread_mutex_unlock(&tlbLock);
    return -1;
}

/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
//pte_t *
void* check_TLB(void *va) {
    pthread_mutex_lock(&tlbLock);
    /* Part 2: TLB lookup code here */
    struct tlb_node* temp = tlb_store->head;
    while(temp != NULL){
        if(temp->vp == (AddressToIndex(va)+1)){
            hit++;
            //printf("hit\n");
            pthread_mutex_unlock(&tlbLock);
            return temp->pa;
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&tlbLock);
    //printf("miss\n");
    return NULL;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;
    miss_rate = (miss/translations);
    /*Part 2 Code here to calculate and print the TLB miss rate*/
    fprintf(stderr, "TLB miss rate %lf  \n", miss_rate);
}


/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
void* Translate(pde_t *pgdir, void *va) {
    translations++;
    if(page_dir == NULL || va == NULL){
        return NULL;
    }
    if(va!=NULL){
        int index = AddressToIndex(va);
        pthread_mutex_lock(&Vbitmap);
        if(v_bitmap[index]==0){// If va not yet allocated return NULL
            pthread_mutex_unlock(&Vbitmap);
            return NULL;
        }
        pthread_mutex_unlock(&Vbitmap);
    }

    void* p = check_TLB(va);
    if(p != NULL){
        return p;
    }

    miss++;

    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.
    int PDEIndex = getExternalBits(va);
    int PTEIndex = getInternalBits(va);
    int offsetToAdd = getOffset(va);
    pthread_mutex_lock(&PageDir);
    if(page_dir[PDEIndex] == NULL){
        pthread_mutex_unlock(&PageDir);
        return NULL;
    }else{
        if(page_dir[PDEIndex][PTEIndex].phyAddr == NULL){
            pthread_mutex_unlock(&PageDir);
            return NULL;
        }else{
            char* addr = (char*)(page_dir[PDEIndex][PTEIndex].phyAddr + offsetToAdd);
            pthread_mutex_unlock(&PageDir);
            add_TLB(va,((void*)addr-offsetToAdd));
            return (void*)addr;
        }
    }
    // Using the page
    //directory index and page table index get the physical address
    //If translation not successfull
    pthread_mutex_lock(&PageDir);
    return NULL;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
PageMap(pde_t *pgdir, void *va, void *pa)
{
    int PDEIndex = getExternalBits(va);
    int PTEIndex = getInternalBits(va);
    int offsetToAdd = getOffset(va);

    int i;

     int num_entry = power(2,internal_bits);
    pthread_mutex_lock(&PageDir);
    if(page_dir[PDEIndex] == NULL){//Set new page table at this entry in the dir.
        //Create a new page table
        page_table* ptable = (page_table*)malloc(num_entry*sizeof(page_table));
        //page_dir[i] = (page_table*)malloc(pte_size *sizeof(page_table));
        page_dir[PDEIndex] = ptable;
        ptable[PTEIndex].phyAddr = pa;
        add_TLB(va,pa);
        // instead of setting everything NULL, when needed we will allocate it directly.
        /*for(i=0;i<num_entry;i++){
            ptable[i].phyAddr = NULL;
        }*/
        page_dir[PDEIndex] = ptable;

        pthread_mutex_unlock(&PageDir);
        return 0;
    }
    else{
        page_dir[PDEIndex][PTEIndex].phyAddr = pa;
        add_TLB(va,pa);
        pthread_mutex_unlock(&PageDir);
        return 0;
    }
    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    pthread_mutex_unlock(&PageDir);
    return -1;
}


/*Function that gets the next available page
*/
int *get_next_avail(int num_pages) {
    //printf("VBITMAP");
    //Use virtual address bitmap to find the next free page
    int* arr = (int*)malloc(num_pages*sizeof(int));
    int i;
    int k =0;
    pthread_mutex_lock(&Vbitmap);
    for(i=0;i<vp;i++){
        if(v_bitmap[i] == 0){
            if(num_pages == 1){
                v_bitmap[i]=1;
                arr[k]= i;
                pthread_mutex_unlock(&Vbitmap);
                return arr;
            }

            int j = i+1;
            int count = 0;

            while(j < vp){//Check for contiguous pages

                if(v_bitmap[j] == 0){

                    count++;
                }

                if(v_bitmap[j] == 1){
                    i=j;
                    break;
                }

                if(count == num_pages-1){
                    count = 0;
                    j = i;
                    while(count<num_pages){
                        v_bitmap[j] = 1;
                        arr[k] = j;
                        j++;
                        count++;
                        k++;
                    }
                    pthread_mutex_unlock(&Vbitmap);
                    return arr;
                }

                j+=1;
            }
        }
    }

    pthread_mutex_unlock(&Vbitmap);
    //Return NULL if couldnt find contiguous pages in virt. mem
    return NULL;
}

int *get_next_availPhy(int num_pages) {

    //Use physical address bitmap to find the next free page
    int i,j = 0;
    int count = 0;
    int first_index;
    int* arr = (int*)malloc(num_pages*sizeof(int));
    pthread_mutex_lock(&Pbitmap);
    for(i=0;i<pp;i++){
        if(p_bitmap[i] == 0){
            if(count == 0){
                first_index = i;
            }
            arr[j] = i;
            j++;
            count++;
        }

        if(count == num_pages){
            j=0;
            while(j<num_pages){
                p_bitmap[arr[j]] = 1;
                j++;
            }
            pthread_mutex_unlock(&Pbitmap);
            return arr;//what happens if just return the array?
        }
    }
    pthread_mutex_unlock(&Pbitmap);
    return NULL;
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *myalloc(unsigned int num_bytes) {
    pthread_mutex_lock(&setMemLock);
    if(init){// if called first time, set up the memory
        SetPhysicalMem();
        init = 0;
    }
    pthread_mutex_unlock(&setMemLock);
    //HINT: If the physical memory is not yet initialized, then allocate and initialize.
    int num_pages = ceil((double)num_bytes/PGSIZE); //if 2.5 pages,we should allocate 3 as 2 will be less.
    if(num_pages == 0){
        num_pages =1;
    }
    //Check for the available pages in get_next_avail for virtual mem
    int * indexVaArr = get_next_avail(num_pages);
    if(indexVaArr == NULL){
        return NULL;
    }
    //Check for the available pages in get_next_availPhy for physical pages
    int* indexPhyArr = get_next_availPhy(num_pages);
    if(indexPhyArr == NULL){
        return NULL;
    }
    int i;
    for(i=0;i<num_pages;i++){
        int x =  PageMap(NULL,indexToAddress(indexVaArr[i]),indexToPhyAddress(indexPhyArr[i]));
        if(x == -1)return NULL;
    }

    /* HINT: If the page directory is not initialized, then initialize the
    page directory. Next, using get_next_avail(), check if there are free pages. If
    free pages are available, set the bitmaps and map a new page. Note, you will
    have to mark which physical pages are used. */

    return (void*)indexToAddress(indexVaArr[0]);
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
int myfree(void *va, int size) {
    if(va == NULL){
        return -1;
    }
    int vaStartIdx = AddressToIndex(va);
    int PagesToFree = ceil((double)size/PGSIZE);
    int vaEndIdx = vaStartIdx+PagesToFree;
    int i;
    pthread_mutex_lock(&Vbitmap);
    for(i= vaStartIdx;vaStartIdx < vaEndIdx; vaStartIdx++){
        if( v_bitmap[vaStartIdx] == 0){
            pthread_mutex_unlock(&Vbitmap);
            return -1;
        }
        int x = freePhyPages(indexToAddress(vaStartIdx));
        if(x==-1){
            pthread_mutex_unlock(&Vbitmap);
            return -1;
        }
        v_bitmap[vaStartIdx] = 0;
    }
    pthread_mutex_unlock(&Vbitmap);
    return 0;
    //Free the page table entries starting from this virtual address (va)
    // Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {
    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/
   if(size < PGSIZE){
    void *pa = Translate(NULL,va);
    memcpy(pa,val,size);
   }else{
     void *tempVA = va;
     int bytesToCpy = 0;
     int ToCpy = 0;
     while(size != 0){
       if(size > PGSIZE){
           ToCpy = getBytesToCpy(tempVA);
       }else{
         ToCpy = size; 
       }
      void *pa = Translate(NULL,tempVA);
      memcpy(pa,val+bytesToCpy,ToCpy);
      bytesToCpy+=ToCpy;
      size = size-ToCpy;
      tempVA = indexToAddress(AddressToIndex(tempVA) + 1);
     }
   }

}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {
   if(size<PGSIZE){
    void *pa = Translate(NULL,va);
    memcpy(val,pa,size);
   }
   else{
     void *tempVA = va;
     int bytesToCpy = 0;
     int ToCpy = 0;
     while(size != 0){
       if(size > PGSIZE){
           ToCpy = getBytesToCpy(tempVA);
       }else{
         ToCpy = size; 
       }
      void *pa = Translate(NULL,tempVA);
      memcpy(val+bytesToCpy,pa,ToCpy);
      bytesToCpy+=ToCpy;
      size = size-ToCpy;
      tempVA = indexToAddress(AddressToIndex(tempVA) + 1);
     }
   }
    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */


}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void MatMult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    matrix accessed. Similar to the code in test.c, you will use GetVal() to
    load each element and perform multiplication. Take a look at test.c! In addition to
    getting the values from two matrices, you will perform multiplication and
    store the result to the "answer array"*/
    int i,j,k;
    int address_a =0, address_b=0;
    int address_ans =0;
    int sum;
    int mat1_i_k;
    int mat2_k_j;
    int ans_i_j;
    for(i=0;i<size;i++){
        for(j=0;j<size;j++){
            sum = 0;
            for(k=0;k<size;k++){
                address_a = (unsigned int)mat1 + ((i*size*sizeof(int)))+(k *sizeof(int));
                address_b = (unsigned int)mat2 + ((k*size*sizeof(int)))+(j*sizeof(int));
                GetVal((void*)address_a,&mat1_i_k,sizeof(int));
                GetVal((void*)address_b,&mat2_k_j,sizeof(int));
                sum = sum + mat1_i_k*mat2_k_j;
            }
            address_ans = (unsigned int)answer+ ((i*size*sizeof(int)))+(j*sizeof(int));
            PutVal((void*)address_ans,&sum,sizeof(sum));
        }
    }

}



/*int main(int argc, char **argv){

    SetPhysicalMem();
    // int addr = 0x1000;
    // void* va = &addr;
    // //Translate(NULL, va);
    // void* a = get_next_avail(3);
    // if(a == NULL){
    //     printf("null");
    // }

    // void* p = get_next_availPhy(3);

    // int x = PageMap(NULL, a, p);

    return 0;
}*/

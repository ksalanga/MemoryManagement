#include "my_vm.h"

/*
Function responsible for allocating and setting your physical memory
*/
void set_physical_mem()
{

    // Allocate physical memory using mmap or malloc; this is the total size of
    // your memory you are simulating

    // HINT: Also calculate the number of physical and virtual pages and allocate
    // virtual and physical bitmaps and initialize them
    physical_mem = mmap(NULL, MEMSIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    if (physical_mem == MAP_FAILED)
    {
        printf("Setting Physical Memory Error");
    }

    // initialize physical and virtual bitmaps for tracking
    physical_bitmap = (char *)malloc(PHYSICAL_BITMAP_SIZE);
    memset(physical_bitmap, 0, PHYSICAL_BITMAP_SIZE);

    // page directory
    set_bit_at_index(physical_bitmap, 0);

    virtual_bitmap = (char *)malloc(VIRTUAL_BITMAP_SIZE);
    memset(virtual_bitmap, 0, VIRTUAL_BITMAP_SIZE);

    for (int i = 0; i < TLB_ENTRIES; i++) {
        tlb[i].va = NULL;
        tlb[i].pa = NULL;
    }
}

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa)
{
    va -= get_bottom_bits((unsigned long)va, OFFSET_BIT_SIZE);
    va += 0x1000;
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    pthread_mutex_lock(&tlb_lock);
    tlb_miss++;
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (!tlb[i].va) {
            tlb[i].va = va;
            tlb[i].pa = pa;
            pthread_mutex_unlock(&tlb_lock);
            return i;
        }
    }

    // Eviction Policy
    time_t t;
    srand((unsigned) time(&t));

    int i = rand() % TLB_ENTRIES;

    tlb[i].va = va;
    tlb[i].pa = pa;
    pthread_mutex_unlock(&tlb_lock);

    return i;
}

void remove_TLB(void *va) {
    va -= get_bottom_bits((unsigned long)va, OFFSET_BIT_SIZE);
    va += 0x1000;
    pthread_mutex_lock(&tlb_lock);
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[i].va == va) {
            tlb[i].va = NULL;
            pthread_mutex_unlock(&tlb_lock);
            return;
        }
    }
    pthread_mutex_unlock(&tlb_lock);
}

/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t * check_TLB(void *va)
{
    va -= get_bottom_bits((unsigned long)va, OFFSET_BIT_SIZE);
    va += 0x1000;
    pthread_mutex_lock(&tlb_lock);
    tlb_lookups++;
    /* Part 2: TLB lookup code here */
    for(int i = 0; i < TLB_ENTRIES;i++){
        if(tlb[i].va == va){
            pte_t *tlb_entry = (pte_t *) tlb[i].pa;
            pthread_mutex_unlock(&tlb_lock);
            return tlb_entry;
        }
    }
    pthread_mutex_unlock(&tlb_lock);

    return NULL;
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    double miss_rate = 0;

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    miss_rate = (tlb_miss/tlb_lookups) * 100;

    fprintf(stdout, "TLB miss rate: %lf%%\n", miss_rate);
}

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va)
{
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
     * 2nd-level-page table index using the virtual address.  Using the page
     * directory index and page table index get the physical address.
     *
     * Part 2 HINT: Check the TLB before performing the translation. If
     * translation exists, then you can return physical address from the TLB.
     */

    //Part 2:
    pte_t *pa = check_TLB(va);

    if (pa) {
        return pa;
    }
    
    // Part 1:
    unsigned long vpn = get_top_bits((unsigned long)va, VPN_BIT_SIZE, SYSTEM_BIT_SIZE);
    unsigned long offset = get_bottom_bits((unsigned long)va, OFFSET_BIT_SIZE);

    unsigned long page_directory_index = get_top_bits(vpn, PAGE_DIRECTORY_BIT_SIZE, VPN_BIT_SIZE);

    unsigned long page_table_index = get_bottom_bits(vpn, PAGE_TABLE_BIT_SIZE);

    pde_t page_directory_entry = *(pgdir + page_directory_index);

    if (page_directory_entry)
    {
        pte_t *page_table = (pte_t *)page_directory_entry;

        pte_t page_table_entry = *(page_table + page_table_index);

        if (page_table_entry)
        {
            add_TLB(va, (void *)page_table_entry);

            return (pte_t *)page_table_entry; //<-physical address?
        }
        return NULL;
    }

    // If translation not successful, then return NULL
    return NULL;
}

/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    va -= 0x1000;

    tlb_lookups++;
    add_TLB(va, pa);

    unsigned long vpn = get_top_bits((unsigned long)va, VPN_BIT_SIZE, SYSTEM_BIT_SIZE);

    unsigned long page_directory_index = get_top_bits(vpn, PAGE_DIRECTORY_BIT_SIZE, VPN_BIT_SIZE);

    unsigned long page_table_index = get_bottom_bits(vpn, PAGE_TABLE_BIT_SIZE);

    pde_t page_directory_entry = *(pgdir + page_directory_index);

    pte_t *page_table = (pte_t *)page_directory_entry;

    *(page_table + page_table_index) = (pte_t)pa;

    return 0;
}

virtual_page get_next_mult_avail(int num_pages)
{
    virtual_page first_free_virtual_page;
    first_free_virtual_page.address = NULL;

    pthread_mutex_lock(&bitmap_lock);
    for (int i = 0; i < NUM_VIRTUAL_PAGES; i++)
    {
        if (get_bit_at_index(virtual_bitmap, i) == 0)
        {
            int success = 1;
            for (int j = i; j < i + num_pages; j++)
            {
                if (get_bit_at_index(virtual_bitmap, j))
                {
                    success = 0;
                    break;
                }
            }

            if (success)
            {
                first_free_virtual_page.address = bitmap_index_to_va(i);
                first_free_virtual_page.bitmap_index = i;

                for (int va_index = i; va_index < i + num_pages; va_index++)
                {
                    set_bit_at_index(virtual_bitmap, va_index);

                    if (va_index % ((int)PAGE_TABLE_ENTRIES) == 0
                        && !allocate_inner_page(va_index)
                    ) {
                        first_free_virtual_page.address = NULL;
                        break;
                    }
                }
                break;
            }
        }
    }
    pthread_mutex_unlock(&bitmap_lock);

    return first_free_virtual_page;
}
/*Function that gets the next available page
 */
virtual_page get_next_avail()
{
    virtual_page free_virtual_page;

    free_virtual_page.address = NULL;
    free_virtual_page.bitmap_index = -1;

    int i;

    pthread_mutex_lock(&bitmap_lock);
    for (i = 0; i < NUM_VIRTUAL_PAGES; i++)
    {
        if (!get_bit_at_index(virtual_bitmap, i))
        {
            free_virtual_page.address = bitmap_index_to_va(i);
            free_virtual_page.bitmap_index = i;
            set_bit_at_index(virtual_bitmap, i);
            break;
        }
    }

    if (i % ((int)PAGE_TABLE_ENTRIES) == 0
    && !allocate_inner_page(i)) {
        free_virtual_page.address = NULL;
    }

    pthread_mutex_unlock(&bitmap_lock);

    return free_virtual_page;
}

int allocate_inner_page(int bitmap_index) {
    int page_directory_index = bitmap_index / ((int) PAGE_TABLE_ENTRIES);

    pde_t *pgdir = (pde_t *)physical_mem;
    pde_t page_directory_entry = *(pgdir + page_directory_index);

    if (!page_directory_entry) {
        physical_page inner_page_physical_entry = get_next_phys(0);

        page_directory_entry = (pde_t)inner_page_physical_entry.address;

        if (page_directory_entry)
        {
            *(pgdir + page_directory_index) = page_directory_entry;
            return 1;
        }
        return 0;
    }
}

physical_page get_next_phys(int lock)
{
    physical_page free_physical_page;
    free_physical_page.address = NULL;

    if (lock)
        pthread_mutex_lock(&bitmap_lock);
    
    for (int i = 1; i < NUM_PHYSICAL_PAGES; i++)
    {
        if (!get_bit_at_index(physical_bitmap, i))
        {
            free_physical_page.address = physical_mem + i * (int)PGSIZE;
            free_physical_page.bitmap_index = i;
            set_bit_at_index(physical_bitmap, i);
            break;
        }
    }

    if (lock)
        pthread_mutex_unlock(&bitmap_lock);

    return free_physical_page;
}

/* Function responsible for allocating pages
and used by the benchmark
*/

// part- initializer function
// If phys_mem = null or pg_dir = null <-Pre-initialization
//   read project 1 for efficient bit map implementation and allocation
//   malloc page directory bits directly and call set physical mem
// If get_next_avail() = true <- there are free pages
//   call page map with vp = return val of get_next_avail(), pp = use physical bitmap to find next avail, pg dir should be static or still atainable
void *t_malloc(unsigned int num_bytes)
{

    /*
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

    /*
     * HINT: If the page directory is not initialized, then initialize the
     * page directory. Next, using get_next_avail(), check if there are free pages. If
     * free pages are available, set the bitmaps and map a new page. Note, you will
     * have to mark which physical pages are used.
     */

    pthread_mutex_lock(&bitmap_lock);
    if (!physical_mem)
    {
        set_physical_mem();
    }
    pthread_mutex_unlock(&bitmap_lock);

    int num_pages = (double)ceil(((double)num_bytes) / PGSIZE) + 1e-9;

    virtual_page fvp;

    if (num_pages > 1)
    {
        int clear = 0;
        struct Queue *phys_bitmap_indexes = createQueue();

        fvp = get_next_mult_avail(num_pages);

        if (fvp.address != NULL)
        {
            virtual_page cvp = fvp;

            for (int i = 0; i < num_pages; i++)
            {
                physical_page pp = get_next_phys(1);

                if (pp.address != NULL)
                {
                    enQueue(phys_bitmap_indexes, pp.bitmap_index);

                    page_map(physical_mem, cvp.address, pp.address);

                    cvp.bitmap_index++;
                    cvp.address = bitmap_index_to_va(cvp.bitmap_index);
                }
                else
                {
                    cvp = fvp;
                    // clear/free inner page entries
                    pthread_mutex_lock(&bitmap_lock);
                    for (int j = 0; j < i; j++) {
                        cvp.address -= 0x1000;

                        unsigned long vpn = get_top_bits((unsigned long)cvp.address, VPN_BIT_SIZE, SYSTEM_BIT_SIZE);
                        unsigned long page_directory_index = get_top_bits(vpn, PAGE_DIRECTORY_BIT_SIZE, VPN_BIT_SIZE);
                        unsigned long page_table_index = get_bottom_bits(vpn, PAGE_TABLE_BIT_SIZE);

                        free_pages(page_directory_index, page_table_index, cvp.bitmap_index);

                        cvp.bitmap_index++;
                        cvp.address = bitmap_index_to_va(cvp.bitmap_index);
                    }
                    pthread_mutex_unlock(&bitmap_lock);
                    fvp.address = NULL;
                    clear = 1;
                    break;
                }
            }
        } else {
            clear = 1;
        }

        if (clear)
        {
            pthread_mutex_lock(&bitmap_lock);
            for (int i = 0; i < num_pages; i++)
            {
                clear_bit_at_index(virtual_bitmap, fvp.bitmap_index);
                fvp.bitmap_index++;
            }
            pthread_mutex_unlock(&bitmap_lock);
        }

        clean_p_bitmap_index_q(phys_bitmap_indexes, clear);
    }
    else
    {
        fvp = get_next_avail();

        physical_page pp = get_next_phys(1);

        if (fvp.address != NULL && pp.address != NULL)
        {
            page_map(physical_mem, fvp.address, pp.address);
        }
        else
        {
            pthread_mutex_lock(&bitmap_lock);

            if (fvp.address != NULL) {
                clear_bit_at_index(virtual_bitmap, fvp.bitmap_index);
            }    
            
            if (pp.address != NULL) {
                clear_bit_at_index(physical_bitmap, pp.bitmap_index);
            }

            pthread_mutex_unlock(&bitmap_lock);

            fvp.address = NULL;
        }
    }

    return fvp.address;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
 */
void t_free(void *va, int size)
{

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */

    va -= 0x1000;

    // Part 1

    if ((unsigned long)(va + size) < MAX_MEMSIZE)
    {
        int num_pages = (double)ceil(((double)size) / PGSIZE) + 1e-9;

        unsigned long starting_vpn = get_top_bits((unsigned long)va, VPN_BIT_SIZE, SYSTEM_BIT_SIZE);
        unsigned long starting_page_directory_index = get_top_bits(starting_vpn, PAGE_DIRECTORY_BIT_SIZE, VPN_BIT_SIZE);
        unsigned long starting_page_table_index = get_bottom_bits(starting_vpn, PAGE_TABLE_BIT_SIZE);

        int starting_bitmap_index = starting_page_directory_index * PAGE_TABLE_ENTRIES + starting_page_table_index;

        free_pages(starting_page_directory_index, starting_page_table_index, starting_bitmap_index);

        for (int va_index = starting_bitmap_index + 1; va_index < starting_bitmap_index + num_pages; va_index++)
        {
            void *current_va = bitmap_index_to_va(va_index);

            current_va -= 0x1000;

            unsigned long current_vpn = get_top_bits((unsigned long)current_va, VPN_BIT_SIZE, SYSTEM_BIT_SIZE);
            unsigned long current_page_directory_index = get_top_bits(current_vpn, PAGE_DIRECTORY_BIT_SIZE, VPN_BIT_SIZE);
            unsigned long current_page_table_index = get_bottom_bits(current_vpn, PAGE_TABLE_BIT_SIZE);

            free_pages(current_page_directory_index, current_page_table_index, va_index);
        }
    }
}

void free_pages(unsigned long page_directory_index, unsigned long page_table_index, int virtual_bitmap_index)
{
    pde_t page_directory_entry = *((pde_t *)physical_mem + page_directory_index);

    pte_t *page_table = (pte_t *)page_directory_entry;

    pthread_mutex_lock(&bitmap_lock);
    if (page_table) {
        pte_t page_table_entry = *((pte_t *)page_table + page_table_index);

        if (page_table_entry) {
            clear_bit_at_index(physical_bitmap, ((void *)page_table_entry - physical_mem) / PGSIZE);

            *((pte_t *)page_table + page_table_index) = 0;
            clear_bit_at_index(virtual_bitmap, virtual_bitmap_index);
        }
    }

    remove_TLB(bitmap_index_to_va(virtual_bitmap_index) - 0x1000);

    pthread_mutex_unlock(&bitmap_lock);
}

/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
 */
void put_value(void *va, void *val, int size)
{

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
    va -= 0x1000;
    unsigned long vpn = get_top_bits((unsigned long)va, VPN_BIT_SIZE, SYSTEM_BIT_SIZE);
    unsigned long page_directory_index = get_top_bits(vpn, PAGE_DIRECTORY_BIT_SIZE, VPN_BIT_SIZE);
    unsigned long page_table_index = get_bottom_bits(vpn, PAGE_TABLE_BIT_SIZE); 

    int starting_bitmap_index = page_directory_index * PAGE_TABLE_ENTRIES + page_table_index;

    pte_t* pa = translate(physical_mem, va);

    unsigned long offset = get_bottom_bits((unsigned long)va, OFFSET_BIT_SIZE);

    int byte_size = min(((int) PGSIZE) - offset, size);

    memcpy(pa + offset, val, byte_size);
    
    size -= byte_size;
    int i = byte_size;
    
    while (size > 0) {
        starting_bitmap_index++;
        va = bitmap_index_to_va(starting_bitmap_index);
        va -= 0x1000;

        pte_t* pa = translate(physical_mem, va);

        byte_size = min(((int) PGSIZE) - offset, size);

        memcpy(pa, (val + i), byte_size);

        size -= byte_size;
        i += byte_size;
    }
}

/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size)
{

    /* HINT: put the values pointed to by "va" inside the physical memory at given
     * "val" address. Assume you can access "val" directly by derefencing them.
     */
    
    va -= 0x1000;
    unsigned long vpn = get_top_bits((unsigned long)va, VPN_BIT_SIZE, SYSTEM_BIT_SIZE);
    unsigned long page_directory_index = get_top_bits(vpn, PAGE_DIRECTORY_BIT_SIZE, VPN_BIT_SIZE);
    unsigned long page_table_index = get_bottom_bits(vpn, PAGE_TABLE_BIT_SIZE); 

    int starting_bitmap_index = page_directory_index * PAGE_TABLE_ENTRIES + page_table_index;

    pte_t* pa = translate(physical_mem, va);

    unsigned long offset = get_bottom_bits((unsigned long)va, OFFSET_BIT_SIZE);

    int byte_size = min(((int) PGSIZE) - offset, size);

    memcpy(val, pa + offset, byte_size);
    
    size -= byte_size;
    int i = byte_size;
    
    while (size > 0) {
        starting_bitmap_index++;
        va = bitmap_index_to_va(starting_bitmap_index);
        va -= 0x1000;

        pte_t* pa = translate(physical_mem, va);

        byte_size = min(((int) PGSIZE) - offset, size);

        memcpy((val + i), pa, byte_size);

        size -= byte_size;
        i += byte_size;
    }
}

/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer)
{

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to
     * getting the values from two matrices, you will perform multiplication and
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++)
    {
        for (j = 0; j < size; j++)
        {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++)
            {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value((void *)address_a, &a, sizeof(int));
                get_value((void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n",
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

void *bitmap_index_to_va(int i) // returns address + 0x1000 because VA starts at 0x1000
{
    unsigned long page_directory_index = i / ((int)PAGE_TABLE_ENTRIES);
    unsigned long page_table_index = i % ((int)PAGE_TABLE_ENTRIES);

    unsigned long VPN = page_directory_index;
    VPN <<= PAGE_TABLE_BIT_SIZE;
    VPN += page_table_index;

    return (void *)((VPN << OFFSET_BIT_SIZE) + 0x1000);
}

static int get_msb_index(unsigned long value)
{
    unsigned long msbIndex = 0;
    while (value >>= 1)
    {
        msbIndex++;
    }
    return msbIndex;
}

static unsigned long get_top_bits(unsigned long value, int num_bits, int binary_length)
{
    return value >> (binary_length - num_bits);
}

static unsigned long get_bottom_bits(unsigned long value, int num_bits)
{
    return value & ((1 << num_bits) - 1);
}

static void set_bit_at_index(char *bitmap, int index)
{
    // Assuming Little Endian Order
    // Go to index divided by 8,
    // Remainder is how many we'll shift and toggle.

    bitmap[index / 8] |= (1UL << (index % 8));
    return;
}

static void clear_bit_at_index(char *bitmap, int index)
{
    // Assuming Little Endian Order
    // Go to index divided by 8,
    // Remainder is how many we'll shift and toggle.

    bitmap[index / 8] &= ~(1UL << (index % 8));
    return;
}

static int get_bit_at_index(char *bitmap, int index)
{
    // Get bits / 8 index,
    // Right shift by remainder
    // Mask with & 1
    int bit = bitmap[index / 8];
    bit >>= (index % 8);
    return bit & 1;
}

void clean_p_bitmap_index_q(struct Queue *q, int clear)
{
    if (clear)
        pthread_mutex_lock(&bitmap_lock);

    while (!isEmpty(q))
    {
        int physical_bitmap_index = deQueue(q);
        if (clear)
        {
            clear_bit_at_index(physical_bitmap, physical_bitmap_index);
        }
    }

    if (clear)
        pthread_mutex_unlock(&bitmap_lock);

    free(q);
}

struct QNode *newNode(int k)
{
    struct QNode *temp = (struct QNode *)malloc(sizeof(struct QNode));
    temp->key = k;
    temp->next = NULL;
    return temp;
}

struct Queue *createQueue()
{
    struct Queue *q = (struct Queue *)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    return q;
}

void enQueue(struct Queue *q, int k)
{
    struct QNode *temp = newNode(k);

    if (q->rear == NULL)
    {
        q->front = q->rear = temp;
        return;
    }

    q->rear->next = temp;
    q->rear = temp;
}

int deQueue(struct Queue *q)
{
    if (q->front == NULL)
        return -1;

    struct QNode *temp = q->front;

    int value = temp->key;

    q->front = q->front->next;

    if (q->front == NULL)
        q->rear = NULL;

    free(temp);
    return value;
}

int isEmpty(struct Queue *q)
{
    if (q->front == NULL)
    {
        return 1;
    }
    return 0;
}

unsigned int log_2(int i)
{
    int l = 0;
    while (i >>= 1)
    {
        ++l;
    };
    return l;
}

int min(int a, int b) {
    return (a > b) ? b : a;
};
#define _GNU_SOURCE

#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>

// Assume the address space is 32 bits, so the max memory size is 4GB
// Page size is 4KB

// Add any important includes here which you may need
#include <math.h>
#include <string.h>
#include <pthread.h>

// Page Sizes:
// 4096, 8192, 16384, 32768, 65536
#define PGSIZE 65536

#define LEVELS 2

// Virtual Address Bits
#define SYSTEM_BIT_SIZE sizeof(unsigned long) * 8

#define PAGE_TABLE_ENTRIES PGSIZE / sizeof(unsigned long)

#define OFFSET_BIT_SIZE log_2(PGSIZE)

#define VPN_BIT_SIZE SYSTEM_BIT_SIZE - OFFSET_BIT_SIZE

#define PAGE_TABLE_BIT_SIZE log_2(PAGE_TABLE_ENTRIES)

#define PAGE_DIRECTORY_BIT_SIZE VPN_BIT_SIZE - ((LEVELS - 1) * PAGE_TABLE_BIT_SIZE)

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL * 1024 * 1024 * 1024

// Size of "physcial memory"
#define MEMSIZE 1024 * 1024 * 1024

#define NUM_VIRTUAL_PAGES MAX_MEMSIZE / PGSIZE

#define NUM_PHYSICAL_PAGES MEMSIZE / PGSIZE

// bitmap size represents the number of "characters" we allocate.
// number of characters (bitmap size) * 8 bits = number of bits
// 1 bit = 1 page
#define PHYSICAL_BITMAP_SIZE NUM_PHYSICAL_PAGES / 8

#define VIRTUAL_BITMAP_SIZE NUM_VIRTUAL_PAGES / 8

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

void *physical_mem; // starting point for allocation of physical memory

char *physical_bitmap;

char *virtual_bitmap; // index 0 is the start of the page directory

// Structure to represents TLB
struct tlb_entry
{
    void *va;
    void *pa;
};

double tlb_miss;
double tlb_lookups;

struct tlb_entry tlb[TLB_ENTRIES];

void remove_TLB(void *va);

typedef struct PHYSICAL_PAGE
{
    void *address;
    int bitmap_index;
} physical_page;

typedef struct VIRTUAL_PAGE
{
    void *address;
    int bitmap_index;
} virtual_page;

// Free Pages
void free_pages(unsigned long page_directory_index, unsigned long page_table_index, int virtual_bitmap_index);

// Conversion
void *bitmap_index_to_va(int i);

// Bits
static int get_msb_index(unsigned long value);

static unsigned long get_top_bits(unsigned long value, int num_bits, int binary_length);
static unsigned long get_bottom_bits(unsigned long value, int num_bits);
static void set_bit_at_index(char *bitmap, int index);
static void clear_bit_at_index(char *bitmap, int index);
static int get_bit_at_index(char *bitmap, int index);

// Gets next physical page
physical_page get_next_phys(int lock);

// Allocates inner page if page directory at that index / PAGE_TABLE_ENTRIES hasn't been allocated yet
int allocate_inner_page(int bitmap_index);

// locks
static pthread_mutex_t bitmap_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tlb_lock = PTHREAD_MUTEX_INITIALIZER;

// log
unsigned int log_2(int i);

// Queue Structure
struct QNode
{
    int key;
    struct QNode *next;
};
struct Queue
{
    struct QNode *front, *rear;
};
struct QNode *newNode(int k);
struct Queue *createQueue();
void enQueue(struct Queue *q, int k);
int deQueue(struct Queue *q);
int isEmpty(struct Queue *q);

void clean_p_bitmap_index_q(struct Queue *q, int clear);

void set_physical_mem();
pte_t *translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void *pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
void put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();

#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../my_vm.h"

#define SIZE 5
#define ARRAY_SIZE 400

void test();
void print_virtual_address_bits();
void translate_test();

int main()
{
    void *pointers[3];
    for (int i = 0; i < 3; i++) {
        pointers[i] = t_malloc(10000);
        printf("%p\n", pointers[i]);
    }

    for (int i = 0; i < 3; i++) {
        t_free(pointers[i], 10000);
    }
    printf("\n");

    return 0;
}

void translate_test()
{
    // Physical Memory Initialization
    int p_i_index = 25;
    pde_t page_inner_address = (pde_t)((void *)physical_mem + (p_i_index * PGSIZE));

    int p_f_index = 97;
    pde_t page_frame_address = (pde_t)((void *)physical_mem + (p_f_index * PGSIZE));

    // Page Directory
    pde_t *pgdir = (pde_t *)physical_mem;
    *(pgdir + 1000) = page_inner_address;

    // Inner Page
    pte_t *pg_inner = (pte_t *)page_inner_address;
    *(pg_inner + 599) = page_frame_address;

    void *va = (void *)0xFA2570FF;

    pte_t *translated_physical_page = translate(pgdir, va);

    char words[700];

    words[255] = 'Q';
    memcpy((void *)page_frame_address, words, 700);

    if (translated_physical_page == (pte_t *)(page_frame_address + 0xFF))
    {
        printf("Valid Translation\n");
        printf("%c\n", (char)*translated_physical_page);
    }
    else
    {
        printf("Invalid Translation\n");
    }

    printf("Page Frame: %p\nTranslated Physical Address: %p\n", (void *)page_frame_address, (void *)translated_physical_page);
}

void print_virtual_address_bits()
{
    printf("SYSTEM_BIT_SIZE: %d\n", SYSTEM_BIT_SIZE);
    printf("OFFSET_BIT_SIZE: %d\n", OFFSET_BIT_SIZE);
    printf("VPN_BIT_SIZE: %d\n", VPN_BIT_SIZE);
    printf("PAGE_INNER_BIT_SIZE: %d\n", PAGE_TABLE_BIT_SIZE);
    printf("PAGE_DIRECTORY_BIT_SIZE: %d\n", PAGE_DIRECTORY_BIT_SIZE);
}

void test()
{
    printf("Allocating three arrays of %d bytes\n", ARRAY_SIZE);

    void *a = t_malloc(ARRAY_SIZE);
    int old_a = (int)a;
    void *b = t_malloc(ARRAY_SIZE);
    void *c = t_malloc(ARRAY_SIZE);
    int x = 1;
    int y, z;
    int i = 0, j = 0;
    int address_a = 0, address_b = 0;
    int address_c = 0;

    printf("Addresses of the allocations: %x, %x, %x\n", (int)a, (int)b, (int)c);

    printf("Storing integers to generate a SIZExSIZE matrix\n");
    for (i = 0; i < SIZE; i++)
    {
        for (j = 0; j < SIZE; j++)
        {
            address_a = (unsigned int)a + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            address_b = (unsigned int)b + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            put_value((void *)address_a, &x, sizeof(int));
            put_value((void *)address_b, &x, sizeof(int));
        }
    }

    printf("Fetching matrix elements stored in the arrays\n");

    for (i = 0; i < SIZE; i++)
    {
        for (j = 0; j < SIZE; j++)
        {
            address_a = (unsigned int)a + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            address_b = (unsigned int)b + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            get_value((void *)address_a, &y, sizeof(int));
            get_value((void *)address_b, &z, sizeof(int));
            printf("%d ", y);
        }
        printf("\n");
    }

    printf("Performing matrix multiplication with itself!\n");
    mat_mult(a, b, SIZE, c);

    for (i = 0; i < SIZE; i++)
    {
        for (j = 0; j < SIZE; j++)
        {
            address_c = (unsigned int)c + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            get_value((void *)address_c, &y, sizeof(int));
            printf("%d ", y);
        }
        printf("\n");
    }
    printf("Freeing the allocations!\n");
    t_free(a, ARRAY_SIZE);
    t_free(b, ARRAY_SIZE);
    t_free(c, ARRAY_SIZE);

    printf("Checking if allocations were freed!\n");
    a = t_malloc(ARRAY_SIZE);
    if ((int)a == old_a)
        printf("free function works\n");
    else
        printf("free function does not work\n");
}
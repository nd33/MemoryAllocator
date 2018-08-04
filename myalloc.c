/*	Stuart Norcross - 12/03/10 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/mman.h>
#include "myalloc.h"

#define BLOCK_HEADER_SIZE sizeof(block_head)
#define PAGE_SIZE 4096 // mem page 4 kb of mem

// memory block header
typedef struct head
{
    int size; // used to store payload size and whether allocated or free
    struct head *previous;
    struct head *next;
} block_head;

void *find_first_fit(int size);
char tryCoalesce(void *payload_ptr);

block_head *BEGIN_OF_HEAP; // begining of heap
char memInit = 'n';        // char to use as a boolean
int freeListLen = 0;

// function to initialise my heap
void initMem()
{
    BEGIN_OF_HEAP = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    BEGIN_OF_HEAP->size = PAGE_SIZE;
    BEGIN_OF_HEAP->previous = BEGIN_OF_HEAP;
    BEGIN_OF_HEAP->next = BEGIN_OF_HEAP;
    memInit = 'y';
}

// function to cut a block into
// part to return to user and
// remainder to put into free list
void cut(block_head *fit, int size)
{
    block_head *newBlock;
    // adjusts address of remainder
    newBlock = (block_head *)((char *)fit + BLOCK_HEADER_SIZE + size);
    newBlock->size = fit->size - (size + BLOCK_HEADER_SIZE); // remainder size
    fit->size = BLOCK_HEADER_SIZE + size;                    // adjust size of fitting block
    fit->previous->next = newBlock;
    fit->next->previous = newBlock;
    newBlock->previous = fit->previous;
    newBlock->next = fit->next;
    freeListLen++;
}

//function to return pointer to mem page for user
void *myalloc(int size)
{
    if (memInit == 'n')
        initMem();                             // initialise heap only on first call
    int len = size + BLOCK_HEADER_SIZE;        // add header size to size
    block_head *pointer = find_first_fit(len); // search free list for fit
    // if no big enough in free list is found
    if (pointer == NULL)
    {
        //cater for bigger than one page requests
        if (size > PAGE_SIZE)
        {
            int numOfPages = size / PAGE_SIZE + 1; // + 1 to make sure it is enough
            pointer = mmap(0, numOfPages * PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
            pointer->size = numOfPages * PAGE_SIZE;
            pointer->previous = BEGIN_OF_HEAP;
            pointer->next = BEGIN_OF_HEAP->next;
            BEGIN_OF_HEAP->next = pointer;
            pointer->next->previous = pointer;
        }
        // mmap single page size
        else
        {
            pointer = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, // incremental mem EXTENSION
                           MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
            pointer->size = PAGE_SIZE;
            pointer->previous = BEGIN_OF_HEAP;
            pointer->next = BEGIN_OF_HEAP->next;
            BEGIN_OF_HEAP->next = pointer;
            pointer->next->previous = pointer;
        }
        // cut mem page if big enough
        if (pointer->size > BLOCK_HEADER_SIZE * 2 + size)
        {
            cut(pointer, size);
            return (char *)pointer + BLOCK_HEADER_SIZE;
        }
        if ((long)pointer == -1) // give up if no more mem available
            return NULL;
    }
    else
    {
        // cut free block if possible
        if (pointer->size > BLOCK_HEADER_SIZE * 2 + size)
        {
            cut(pointer, size);
        }
        // give full block to user
        else
        {
            pointer->previous->next = pointer->next;     // set next
            pointer->next->previous = pointer->previous; // set previous
        }
    }
    // return pointer to payload
    return (char *)pointer + BLOCK_HEADER_SIZE;
}

//function to find first fitting block in free list
void *find_first_fit(int size)
{
    block_head *pointer;
    // traverse list
    for (pointer = BEGIN_OF_HEAP->next;
         pointer != BEGIN_OF_HEAP && pointer->size < size;
         pointer = pointer->next);
    if (pointer != BEGIN_OF_HEAP)
    {
        freeListLen--; // substract one el from free list
        return pointer;
    }
    else
        return NULL;
}

// frees mem location and adds it to the free list
void myfree(void *ptr)
{
    // try to coalesce adjacent blocks
    if (tryCoalesce(ptr) != 's')
    {
        block_head *block_pointer = ptr - BLOCK_HEADER_SIZE; // get header
        block_pointer->next = BEGIN_OF_HEAP->next;
        block_pointer->previous = BEGIN_OF_HEAP;
        BEGIN_OF_HEAP->next = block_pointer;
        block_pointer->next->previous = block_pointer;
        freeListLen++;
    }
}

//try to merge adjacent blocks in free list
char tryCoalesce(void *payload_ptr)
{
    char coalesce = 'f'; // 'f' for failure
    int counter = 0;     // debugging coutner to prevent possible infintie loops
    // check if free list is empty
    if (freeListLen > 0)
    {
        // get pointer to current block
        block_head *block_pointer = payload_ptr - BLOCK_HEADER_SIZE;
        block_head *header_after;
        block_head *header_before;
        // traverse through free list in both directions simultaneously
        for (header_after = block_pointer->next,
            header_before = block_pointer->previous;
             header_after != block_pointer && header_after != NULL &&
             header_before != block_pointer && header_before != NULL && counter < 50;
             header_after = header_after->next, header_before = header_before->previous)
        {
            // skip begin of heap page used only for navigation (can be optimised)
            if (header_after != BEGIN_OF_HEAP)
            {
                // get pointer to next block header
                char *next_pointer = ((char *)block_pointer + block_pointer->size);
                // see if next block is adjacent to current
                if ((char *)header_after == next_pointer)
                {
                    block_pointer->size += header_after->size; // add sizes
                    //adjust pointers
                    block_pointer->next = header_after->next;
                    block_pointer->next->previous = block_pointer;
                    coalesce = 's';
                    break;
                }
            }
            // skip begin of heap
            if (header_before != BEGIN_OF_HEAP)
            {
                // get pointer to
                char *previous_pointer = ((char *)block_pointer - header_before->size);
                if ((char *)header_before == previous_pointer)
                {
                    block_pointer->size += header_before->size;
                    block_pointer->next = header_before->next;
                    block_pointer->next->previous = block_pointer;
                    coalesce = 's';
                    break;
                }
            }
            counter++;
        }
    }
    return coalesce;
}

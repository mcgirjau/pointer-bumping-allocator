// ==============================================================================
/**
 * pb-alloc.c
 *
 * A _pointer-bumping_ heap allocator.  This allocator *does not re-use* freed
 * blocks.  It uses _pointer bumping_ to expand the heap with each allocation.
 **/
// ==============================================================================



// ==============================================================================
// INCLUDES

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "safeio.h"
// ==============================================================================



// ==============================================================================
// MACRO CONSTANTS AND FUNCTIONS

/** Word size. */
#define DBL_WORD_SIZE 16

/** The system's page size. */
#define PAGE_SIZE sysconf(_SC_PAGESIZE)

/**
 * Macros to easily calculate the number of bytes for larger scales (e.g., kilo,
 * mega, gigabytes).
 */
#define KB(size)  ((size_t)size * 1024)
#define MB(size)  (KB(size) * 1024)
#define GB(size)  (MB(size) * 1024)

/** The virtual address space reserved for the heap. */
#define HEAP_SIZE GB(2)
// ==============================================================================


// ==============================================================================
// TYPES AND STRUCTURES

/** A header for each block's metadata. */
typedef struct header {

  /** The size of the useful portion of the block, in bytes. */
  size_t size;
  
} header_s;
// ==============================================================================



// ==============================================================================
// GLOBALS

/** The address of the next available byte in the heap region. */
static intptr_t free_addr  = 0;

/** The beginning of the heap. */
static intptr_t start_addr = 0;

/** The end of the heap. */
static intptr_t end_addr   = 0;
// ==============================================================================



// ==============================================================================
/**
 * The initialization method.  If this is the first use of the heap, initialize it.
 */

void init () {

  // Only do anything if there is no heap region (i.e., first time called).
  if (start_addr == 0) {

    DEBUG("Trying to initialize");
    
    // Allocate virtual address space in which the heap will reside. Make it
    // un-shared and not backed by any file (_anonymous_ space).  A failure to
    // map this space is fatal.
    void* heap = mmap(NULL,
		      HEAP_SIZE,
		      PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS,
		      -1,
		      0);
    if (heap == MAP_FAILED) {
      ERROR("Could not mmap() heap region");
    }

    // Hold onto the boundaries of the heap as a whole.
    start_addr = (intptr_t)heap;
    end_addr   = start_addr + HEAP_SIZE;
    free_addr  = start_addr;

    // DEBUG: Emit a message to indicate that this allocator is being called.
    DEBUG("bp-alloc initialized");

  }

} // init ()
// ==============================================================================


// ==============================================================================
/**
 * Allocate and return `size` bytes of heap space.  Expand into the heap region
 * via _pointer bumping_.
 *
 * \param size The number of bytes to allocate.

 * \return A pointer to the allocated block, if successful; `NULL` if
 *         unsuccessful.
 */
void* malloc (size_t size) {
  
  /** Ensure that the heap is initialized. */
  init();

  /** Ensure that the pointer that gets returned is double-word aligned. 
   *  Specifically, free_addr should be sizeof(header_s) away from a
   *  double-word boundary, so that after the header is put in place, the 
   *  usable block is aligned appropriately. */
  intptr_t padding = (sizeof(header_s) + DBL_WORD_SIZE - (free_addr % DBL_WORD_SIZE)) % DBL_WORD_SIZE; 
  free_addr += padding;

  /** If trying to allocate a block of zero length, return a null pointer. */
  if (size == 0) {
    return NULL;
  }

  /** When allocating memory in the heap, ensure we add on some extra space
   *  for the header, besides what the program is requesting. */
  size_t    total_size = size + sizeof(header_s);

  /** header_ptr gets a pointer to the first free address in the heap, where
   *  the header will be located. */
  header_s* header_ptr = (header_s*)free_addr;

  /** block_ptr gets a generic (void*) pointer to the first address of the 
   *  block of memory being allocated. This address comes right after the
   *  header, so we are adding the size of the header to the first free address
   *  in order to find the address of the actual allocated space. */
  void*     block_ptr  = (void*)(free_addr + sizeof(header_s));

  /** new_free_address is where the first free address pointer would point to 
   *  after the allocation. It is basically a translation of free_addr by the
   *  total size of the allocated block (header + space to be used by the 
   *  program). */
  intptr_t new_free_addr = free_addr + total_size;

  /** Have we exceeded the maximum size of the heap? */
  if (new_free_addr > end_addr) {

    /** If yes, then return a null pointer - allocation failed. */
    return NULL;

  } else {

    /** If not, then the first free address gets updated accordingly. */
    free_addr = new_free_addr;

  }

  /** Write the size of the allocated block to that block's header. Note that
   *  this is the size of the actual usable part, not the total size. */
  header_ptr->size = size;
  
  /** Return a pointer to the first address of the program-usable part of the 
   *  allocated space - allocation succeeded. */
  return block_ptr;

} // malloc()
// ==============================================================================



// ==============================================================================
/**
 * Deallocate a given block on the heap.  Add the given block (if any) to the
 * free list.
 *
 * \param ptr A pointer to the block to be deallocated.
 */
void free (void* ptr) {

  DEBUG("free(): ", (intptr_t)ptr);

} // free()
// ==============================================================================



// ==============================================================================
/**
 * Allocate a block of `nmemb * size` bytes on the heap, zeroing its contents.
 *
 * \param nmemb The number of elements in the new block.
 * \param size  The size, in bytes, of each of the `nmemb` elements.
 * \return      A pointer to the newly allocated and zeroed block, if successful;
 *              `NULL` if unsuccessful.
 */
void* calloc (size_t nmemb, size_t size) {

  // Allocate a block of the requested size.
  size_t block_size = nmemb * size;
  void*  block_ptr  = malloc(block_size);

  // If the allocation succeeded, clear the entire block.
  if (block_ptr != NULL) {
    memset(block_ptr, 0, block_size);
  }

  return block_ptr;
  
} // calloc ()
// ==============================================================================



// ==============================================================================
/**
 * Update the given block at `ptr` to take on the given `size`.  Here, if `size`
 * fits within the given block, then the block is returned unchanged.  If the
 * `size` is an increase for the block, then a new and larger block is
 * allocated, and the data from the old block is copied, the old block freed,
 * and the new block returned.
 *
 * \param ptr  The block to be assigned a new size.
 * \param size The new size that the block should assume.
 * \return     A pointer to the resultant block, which may be `ptr` itself, or
 *             may be a newly allocated block.
 */
void* realloc (void* ptr, size_t size) {

  /** If passed in a null pointer, then presumably there's no pre-existent
   *  block. As such, call malloc to allocate a new one of the desired size. */
  if (ptr == NULL) {
    return malloc(size);
  }

  /** If passed a new size of 0, this is basically the same as freeing the
   *  block. So call free and return a null pointer. */
  if (size == 0) {
    free(ptr);
    return NULL;
  }

  /** old_header gets the address of the header of the old (i.e. un-reallocated)
   *  block of memory. This is done by walking backwards from the block pointer
   *  a number of bytes equal to the known header size. */
  header_s* old_header = (header_s*)((intptr_t)ptr - sizeof(header_s));

  /** Read in the size of the old block of memory from the old header. */
  size_t    old_size   = old_header->size;

  /** If the new size the program asks for is less than or equal to the old
   *  size, simply return the old pointer - there's enough space in the current
   *  block already. */
  if (size <= old_size) {
    return ptr;
  }

  /** Otherwise (i.e. if the program is asking for a bigger size than the 
   *  old one), call malloc to allocate a new block of that size somewhere
   *  else that might be available. */
  void* new_ptr = malloc(size);

  /** If the allocation succeeded (i.e. the pointer returned by malloc is not
   *  null), then copy all the contents of the old block into the new block,
   *  and free the old block. */
  if (new_ptr != NULL) {
    memcpy(new_ptr, ptr, old_size);
    free(ptr);
  }

  /** Return a pointer to the newly allocated block of memory. */
  return new_ptr;
  
} // realloc()
// ==============================================================================



#if defined (ALLOC_MAIN)
// ==============================================================================
/**
 * The entry point if this code is compiled as a standalone program for testing
 * purposes.
 */
int main () {

  // Allocate a few blocks, then free them.
  void* x = malloc(16);
  void* y = malloc(64);
  void* z = malloc(32);

  free(z);
  free(y);
  free(x);

  return 0;
  
} // main()
// ==============================================================================
#endif

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define DBL_WORD_SIZE 16

int main (int argc, char **argv) {

  char* x = malloc(24);
  char* y = malloc(19);
  char* z = malloc(32);
  
  printf("x = %p\n", x);
  printf("y = %p\n", y);
  printf("z = %p\n", z);

  // TESTING malloc() ---------------------------------------------------------

  /** For 100 random sizes from 1 to 100 bytes, check double word alignment. */
  for (int i = 0; i < 10; i++) {
    size_t size = (size_t)(rand() % 100);
    char* test = malloc(size);
    assert((intptr_t)test % DBL_WORD_SIZE == 0); // returned address should be double-word aligned
  }

  // TESTING realloc() ---------------------------------------------------------

  /** Test case #1: new size < old size. */
  size_t old_sizes1[10] = {2, 7, 10, 16, 21, 25, 29, 34, 38, 45};
  size_t new_sizes1[10] = {1, 5,  9, 12,  7, 20, 16, 29,  3, 32};
  for (int i = 0; i < 10; i++) {
    char* test1_old = malloc(old_sizes1[i]);
    char* test1_new = realloc(test1_old, new_sizes1[i]);
    assert(test1_new == test1_old);                             // pointer should not change
  }

  /** Test case #2: new size == old size. */
  size_t old_sizes2[10] = {2, 7, 10, 16, 21, 25, 29, 34, 38, 45};
  size_t new_sizes2[10] = {2, 7, 10, 16, 21, 25, 29, 34, 38, 45};
  for (int i = 0; i < 10; i++) {
    char* test2_old = malloc(old_sizes2[i]);
    char* test2_new = realloc(test2_old, new_sizes2[i]);
    assert(test2_new == test2_old);                             // pointer should not change
  }

  /** Test case #3: new size > old size. */
  size_t old_sizes3[10] = {2, 7,  10, 16, 21, 25, 29, 34, 38, 45};
  size_t new_sizes3[10] = {3, 75, 15, 19, 29, 36, 31, 47, 56, 47};
  for (int i = 0; i < 10; i++) {
    char* test3_old = malloc(old_sizes3[i]);
    char* test3_new = realloc(test3_old, new_sizes3[i]);
    assert(test3_new != test3_old);                             // pointer should change
    assert(*test3_new == *test3_old);                           // contents should be copied over
  }
}

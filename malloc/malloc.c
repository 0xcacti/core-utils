#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct block_meta {
  size_t size;
  struct block_meta *next;
  int free;
  int magic; // For debugging only
};

#define META_SIZE sizeof(struct block_meta)

void *global_base = NULL;

struct block_meta *find_free_block(struct block_meta **last, size_t size) {
  struct block_meta *current = global_base;
  while (current && !(current->free && current->size >= size)) {
    *last = current;
    current = current->next;
  }
  return current;
}

struct block_meta *request_space(struct block_meta *last, size_t size) {
  struct block_meta *block;
  block = sbrk(0);
  void *request = sbrk(size + META_SIZE);
  assert((void *)block == request);
  if (request == (void *)-1) {
    return NULL;
  }

  if (last) {
    last->next = block;
  }
  block->size = size;
  block->next = NULL;
  block->free = 0;
  block->magic = 0x12345678;
  return block;
}

struct block_meta *get_block_ptr(void *ptr) {
  return (struct block_meta *)ptr - 1;
}

void free(void *ptr) {
  if (!ptr)
    return;
  struct block_meta *block_ptr = get_block_ptr(ptr);
  assert(block_ptr->free == 0);
  assert(block_ptr->magic == 0x77777777 || block_ptr->magic == 0x12345678);
  block_ptr->free = 1;
  block_ptr->magic = 0x55555555;
}

void *malloc(size_t size) {
  struct block_meta *block;
  if (size <= 0) {
    return NULL;
  }

  if (!global_base) {
    block = request_space(NULL, size);
    if (!block)
      return NULL;
    global_base = block;
  } else {
    struct block_meta *last = global_base;
    block = find_free_block(&last, size);
    if (!block) {
      block = request_space(last, size);
      if (!block)
        return NULL;
    } else {
      block->free = 0;
      block->magic = 0x77777777;
    }
  }
  return (block + 1);
}

void *realloc(void *ptr, size_t size) {
  if (!ptr)
    return malloc(size);

  struct block_meta *old_block = get_block_ptr(ptr);
  if (size <= old_block->size) {
    return ptr;
  }

  void *new_ptr;
  new_ptr = malloc(size);
  if (!new_ptr)
    return NULL;
  memcpy(new_ptr, ptr, old_block->size);
  free(ptr);
  return new_ptr;
}

void *calloc(size_t nelem, size_t elsize) {
  size_t size = nelem * elsize;
  void *ptr = malloc(size);
  memset(ptr, 0, size);
  return ptr;
}

int *make_array_illegal(int n) {
  int arr[n];
  for (int i = 0; i < n; i++) {
    arr[i] = i;
  }
  return arr;
}

int *make_array(int n) {
  int *arr = malloc(n * sizeof(int));
  if (arr == NULL) {
    return NULL;
  }
  for (int i = 0; i < n; i++) {
    arr[i] = i;
  }
  return arr;
}

int main() {
  int n = 5;
  int *a = make_array(n);
  for (int i = 0; i < n; i++) {
    printf("%d\n", a[i]);
  }

  return 0;
}

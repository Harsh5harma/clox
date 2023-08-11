/*
 _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 
|oldSize     |	 newSize              |	Operation                   |
|_ _ _ _ _ _ | _ _ _ _ _ _ _ _ _ _ _  | _ _ _ _ _ _ _ _ _ _ _ _ _ _ |    
|0	         |  Non‑zero	            |  Allocate new block.        |
|Non‑zero	   |  0	                    |  Free allocation.           |
|Non‑zero	   | Smaller than oldSize	  |  Shrink existing allocation.|
|Non‑zero	   |  Larger than oldSize	  |  Grow existing allocation.  |
| _ _ _ _ _ _|_ _ _ _ _ _ _ _ _ _ _ _ | _ _ _ _ _ _ _ _ _ _ _ _ _ _ |
*/

#include <stdlib.h>
#include "../headers/memory.h"


void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  // deallocates memory
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize); // expands memory if oldSize < newSize (0 < non-zero), shrinks if oldSize > newSize 
  if (result == NULL) exit(1);
  return result;
}
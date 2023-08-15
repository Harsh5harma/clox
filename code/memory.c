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
#include "../headers/vm.h"

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

/*
 Since some object types also allocate other memory that they own, 
 we also need a little type-specific code to handle each object type’s special needs.
*/

static void freeObject(Obj* object) {
  switch (object->type) {
    case OBJ_STRING: {
      // We free the character array, then we free the ObjString struct
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length+1);
      FREE(ObjString, object);
      break;
    }
  }
}

void freeObjects() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
}


# StackAllocator and List

A stack-based allocator and custom doubly-linked list with allocator support.

## StackAllocator<T, N>
- **Zero Dynamic Memory**:
  - Uses `StackStorage<N>` for on-stack storage.
  - Supports alignment (e.g., `alignof(double) = 8`).
- **Compatibility**:
  - Works with STL containers (e.g., `std::vector`, `std::list`).

## List<T, Allocator>
- **Features**:
  - Allocator-aware constructors.
  - O(1) `size()`, `push_back()`, `pop_front()`.
  - Bidirectional iterators, reverse iterators.
- **Performance**:
  - Faster than `std::list` with `StackAllocator`.

## Example
```cpp
#include "list.h"
#include "stack_allocator.h"

StackStorage<4096> storage;
List<int, StackAllocator<int, 4096>> list(storage);
list.push_back(42); // No heap allocations!

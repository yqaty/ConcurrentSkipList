#include "allocator.h"

#include <atomic>
#include <cstring>

Allocator::Allocator(int _size) : size(_size) {
  now.store(0, std::memory_order_relaxed);
  buffer = new char[size];
  memset(buffer, 0, size);
}

Allocator::~Allocator() { delete[] buffer; }

void* Allocator::Alloc(int _size) {
  int _now = now.load(std::memory_order_relaxed);
  while (!now.compare_exchange_weak(_now, _now + _size))
    ;
  return reinterpret_cast<void*>(buffer + _now);
}

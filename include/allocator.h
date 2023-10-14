#pragma once
#include <atomic>
const int BUFSIZE = 1 << 25;
class Allocator {
 private:
  char *buffer;
  int size;
  std::atomic<int> now;

 public:
  Allocator(int _size);
  ~Allocator();
  void *Alloc(int size);
};
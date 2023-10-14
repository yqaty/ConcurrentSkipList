#include "skiplist.h"

#include <bits/stdc++.h>

#include "allocator.h"
#include "random.h"

using namespace std;

using List = MySkipList::SkipList;

void test_insert_ordered() {
  Allocator alloc(BUFSIZE);
  List l(&alloc);
  for (int i = 1; i <= 100; ++i) {
    int key = i, value = i;
    if (l.InsertConcurrently(l.AllocateKeyAndValue(key, value))) {
      printf("insert (%d,%d) successfully!\n", key, value);
    } else {
      printf("failed to insert (%d,%d)!\n", key, value);
    }
  }
  for (int i = 1; i <= 200; ++i) {
    int key = i, value = i;
    if (l.InsertConcurrently(l.AllocateKeyAndValue(key, value))) {
      printf("insert (%d,%d) successfully!\n", key, value);
    } else {
      printf("failed to insert (%d,%d)!\n", key, value);
    }
  }
  l.Print();
}

void test_insert_random() {
  Allocator alloc(BUFSIZE);
  List l(&alloc);
  for (int i = 1; i <= 1e6; ++i) {
    int key = MySkipList::rand_(), value = MySkipList::rand_();
    if (l.InsertConcurrently(l.AllocateKeyAndValue(key, value))) {
      printf("insert (%d,%d) successfully!\n", key, value);
    } else {
      printf("failed to insert (%d,%d)!\n", key, value);
    }
  }
  l.Print();
}

void insert_concurrently_ordered(List *l, int id, int st, int ed) {
  void *hint = nullptr;
  for (int i = st; i <= ed; ++i) {
    // printf("%d %d %d %d\n", id, st, ed, i);
    this_thread::sleep_for(std::chrono::milliseconds(1));
    l->InsertWithHintConcurrently(l->AllocateKeyAndValue(i, i), &hint);
  }
  delete[] reinterpret_cast<char *>(hint);
}

void test_insert_concurrently_ordered() {
  Allocator alloc(BUFSIZE);
  List l(&alloc);
  thread th[100];
  for (int i = 0; i < 100; ++i) {
    th[i] = thread(insert_concurrently_ordered, &l, i, i * 100, (i + 4) * 100);
  }
  for (int i = 0; i < 100; ++i) {
    th[i].join();
  }
  l.Print();
}

void insert_concurrently_random(List *l, int id, int st, int ed) {
  void *hint = nullptr;
  for (int i = st; i <= ed; ++i) {
    // printf("%d %d %d %d\n", id, st, ed, i);
    this_thread::sleep_for(std::chrono::milliseconds(1));
    l->InsertWithHintConcurrently(
        l->AllocateKeyAndValue(MySkipList::rand_(), MySkipList::rand_()),
        &hint);
  }
  delete[] reinterpret_cast<char *>(hint);
}

void test_insert_concurrently_random() {
  Allocator alloc(BUFSIZE);
  List l(&alloc);
  thread th[100];
  for (int i = 0; i < 100; ++i) {
    th[i] = thread(insert_concurrently_random, &l, i, i * 100, (i + 4) * 100);
  }
  for (int i = 0; i < 100; ++i) {
    th[i].join();
  }
  l.Print();
}

int main() {
  // test_insert_ordered();
  // test_insert_random();
  // test_insert_concurrently_ordered();
  test_insert_concurrently_random();
  return 0;
}
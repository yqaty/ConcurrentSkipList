#include "skiplist.h"

#include <atomic>
#include <cstring>

#include "allocator.h"
#include "macro.h"
#include "random.h"

namespace MySkipList {

void SkipList::Node::StashHeight(const int height) {
  memcpy(static_cast<void*>(&next_[0]), &height, sizeof(height));
}

int SkipList::Node::UnstashHeight() const {
  int res;
  memcpy(&res, &next_[0], sizeof(res));
  return res;
}

const int32_t* SkipList::Node::Key() const {
  return reinterpret_cast<const int32_t*>(&next_[1]);
}

const int32_t* SkipList::Node::Value() const {
  return (reinterpret_cast<const int32_t*>(&next_[1]) + 1);
}

SkipList::Node* SkipList::Node::Next(int n) {
  return (&next_[0] - n)->load(std::memory_order_acquire);
}

void SkipList::Node::SetNext(int n, Node* x) {
  (&next_[0] - n)->store(x, std::memory_order_release);
}

bool SkipList::Node::CASNext(int n, Node* expected, Node* x) {
  return (&next_[0] - n)->compare_exchange_strong(expected, x);
}

SkipList::Node* SkipList::Node::NoBarrier_Next(int n) {
  return (&next_[0] - n)->load(std::memory_order_relaxed);
}

void SkipList::Node::NoBarrier_SetNext(int n, Node* x) {
  (&next_[0] - n)->store(x, std::memory_order_relaxed);
}

SkipList::SkipList(Allocator* allocator, int32_t max_height,
                   int32_t branching_factor)
    : kMaxHeight_(static_cast<uint16_t>(max_height)),
      kBranching_(static_cast<uint16_t>(branching_factor)),
      kScaledInverseBranching_((1 << 30) / kBranching_),
      allocator_(allocator),
      head_(AllocateNode(0, 0, max_height)),
      max_height_(1) {
  for (int i = 0; i < kMaxHeight_; ++i) {
    head_->SetNext(i, nullptr);
  }
}

int SkipList::GetMaxHeight() const {
  return max_height_.load(std::memory_order_relaxed);
}

int SkipList::RandomHeight() {
  int height = 1;
  while (height < kMaxHeight_ && rand_() < kScaledInverseBranching_) {
    ++height;
  }
  return height;
}

int32_t* SkipList::AllocateKeyAndValue(const int32_t key, const int32_t value) {
  return const_cast<int32_t*>(AllocateNode(key, value, RandomHeight())->Key());
}

SkipList::Node* SkipList::AllocateNode(const int32_t key, const int32_t value,
                                       const int height) {
  size_t pre = sizeof(std::atomic<Node*>) * (height - 1);
  char* st = reinterpret_cast<char*>(
      allocator_->Alloc(pre + sizeof(Node) + sizeof(key) + sizeof(value)));
  Node* x = reinterpret_cast<Node*>(st + pre);
  x->StashHeight(height);
  memcpy(st + pre + sizeof(Node), &key, sizeof(key));
  memcpy(st + pre + sizeof(Node) + sizeof(key), &value, sizeof(value));
  return x;
}

SkipList::Splice* SkipList::AllocateSpliceOnHeap() {
  size_t array_siz = sizeof(Node*) * (kMaxHeight_ + 1);
  char* raw = new char[sizeof(Splice) + 2 * array_siz];
  Splice* splice = reinterpret_cast<Splice*>(raw);
  splice->height_ = 0;
  splice->prev_ = reinterpret_cast<Node**>(raw + sizeof(Splice));
  splice->next_ = reinterpret_cast<Node**>(raw + sizeof(Splice) + array_siz);
  return splice;
}

bool SkipList::Equal(const int32_t& a, const int32_t& b) const {
  return a == b;
}

bool SkipList::LessThan(const int32_t& a, const int32_t& b) const {
  return a < b;
}

bool SkipList::KeyIsAfterNode(const int32_t key, Node* n) const {
  return n != nullptr && LessThan(*(n->Key()), key);
}

bool SkipList::KeyIsBeforeNode(const int32_t key, Node* n) const {
  return n == nullptr || LessThan(key, *(n->Key()));
}

bool SkipList::InsertWithHintConcurrently(const int32_t* key, void** hint) {
  Splice* splice = reinterpret_cast<Splice*>(*hint);
  if (splice == nullptr) {
    splice = AllocateSpliceOnHeap();
    *hint = reinterpret_cast<void*>(splice);
  }
  return Insert(key, splice, true);
}

bool SkipList::InsertConcurrently(const int32_t* key) {
  Splice* splice = AllocateSpliceOnHeap();
  bool bo = Insert(key, splice, false);
  delete[] reinterpret_cast<char*>(splice);
  return bo;
}

template <bool prefetch_before>
void SkipList::FindSpliceForLevel(const int32_t key, Node* before, Node* after,
                                  int level, Node** out_prev, Node** out_next) {
  while (true) {
    Node* next = before->Next(level);
    if (next != nullptr) {
      PREFETCH(next->Next(level), 0, 1);
    }
    if (prefetch_before) {
      if (next != nullptr && level != 0) {
        PREFETCH(next->Next(level - 1), 0, 1);
      }
    }
    if (next == after || !KeyIsAfterNode(key, next)) {
      *out_prev = before;
      *out_next = next;
      return;
    }
    before = next;
  }
}

void SkipList::RecomputeSpliceLevels(const int32_t key, Splice* splice,
                                     int recompute_level) {
  for (int i = recompute_level - 1; i >= 0; --i) {
    FindSpliceForLevel<true>(key, splice->prev_[i + 1], splice->next_[i + 1], i,
                             &splice->prev_[i], &splice->next_[i]);
  }
}

bool SkipList::Insert(const int32_t* key, Splice* splice,
                      bool allow_partial_splice_fix) {
  Node* x = reinterpret_cast<Node*>(const_cast<int32_t*>(key)) - 1;
  int height = x->UnstashHeight();
  int max_height = max_height_.load(std::memory_order_relaxed);
  while (height > max_height) {
    if (max_height_.compare_exchange_weak(max_height, height)) {
      max_height = height;
      break;
    }
  }

  int recompute_height = 0;
  if (splice->height_ < max_height) {
    splice->height_ = max_height;
    recompute_height = max_height;
    splice->prev_[max_height] = head_;
    splice->next_[max_height] = nullptr;
  } else {
    while (recompute_height < max_height) {
      if (splice->prev_[recompute_height]->Next(recompute_height) !=
          splice->next_[recompute_height]) {
        ++recompute_height;
      } else if (splice->prev_[recompute_height] != head_ &&
                 !KeyIsAfterNode(*key, splice->prev_[recompute_height])) {
        if (allow_partial_splice_fix) {
          Node* bad = splice->prev_[recompute_height];
          while (splice->prev_[recompute_height] == bad) {
            ++recompute_height;
          }
        } else {
          recompute_height = max_height;
        }
      } else if (KeyIsAfterNode(*key, splice->next_[recompute_height])) {
        if (allow_partial_splice_fix) {
          Node* bad = splice->next_[recompute_height];
          while (splice->next_[recompute_height] == bad) {
            ++recompute_height;
          }
        } else {
          recompute_height = max_height;
        }
      } else {
        break;
      }
    }
  }
  if (recompute_height > 0) {
    RecomputeSpliceLevels(*key, splice, recompute_height);
  }

  bool splice_is_vaild = true;
  for (int i = 0; i < height; ++i) {
    while (true) {
      if (UNLIKELY(i == 0 && splice->prev_[0] != head_ &&
                   (!KeyIsAfterNode(*key, splice->prev_[0])))) {
        return false;
      }
      if (UNLIKELY(i == 0 && splice->next_[0] != nullptr &&
                   (!KeyIsBeforeNode(*key, splice->next_[0])))) {
        return false;
      }
      x->NoBarrier_SetNext(i, splice->next_[i]);
      if (splice->prev_[i]->CASNext(i, splice->next_[i], x)) {
        break;
      }
      FindSpliceForLevel<false>(*key, splice->prev_[i], nullptr, i,
                                &splice->prev_[i], &splice->next_[i]);
    }
    if (i > 0) {
      splice_is_vaild = false;
    }
  }
  if (splice_is_vaild) {
    for (int i = 0; i < height; ++i) {
      splice->prev_[i] = x;
    }
  } else {
    splice->height_ = 0;
  }
  return true;
}

SkipList::Node* SkipList::FindGreaterOrEqual(const int32_t key) const {
  Node* now = head_;
  Node* last_bigger = nullptr;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = now->Next(level);
    if (next != nullptr) {
      PREFETCH(next->Next(level), 0, 1);
    }
    if (next == nullptr || next == last_bigger || !KeyIsAfterNode(key, next)) {
      if (Equal(key, *next->Key()) || level == 0) {
        return next;
      } else {
        --level;
      }
    } else {
      now = next;
    }
  }
}

SkipList::Node* SkipList::Search(const int32_t key) const {
  Node* x = FindGreaterOrEqual(key);
  return x != nullptr && Equal(*x->Key(), key) ? x : nullptr;
}

void SkipList::Print() const {
  Node* now = head_->Next(0);
  while (now != nullptr) {
    printf("%d %d\n", *now->Key(), *now->Value());
    now = now->Next(0);
  }
  putchar('\n');
}

}  // namespace MySkipList
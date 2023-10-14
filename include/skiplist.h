#pragma once

#include <atomic>

#include "allocator.h"

namespace MySkipList {

class SkipList {
 public:
  struct Node;
  struct Splice;

  static const uint16_t kMaxPossibleHeight = 32;

  explicit SkipList(Allocator* allocator, int32_t max_height = 12,
                    int32_t branching_factor = 4);

  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;

  int32_t* AllocateKeyAndValue(const int32_t key, const int32_t value);

  Splice* AllocateSpliceOnHeap();

  bool InsertWithHintConcurrently(const int32_t* key, void** hint);

  bool InsertConcurrently(const int32_t* key);

  bool Insert(const int32_t* key, Splice* splice,
              bool allow_partial_splice_fix);

  Node* Search(const int32_t key) const;

  void Print() const;

 private:
  const uint16_t kMaxHeight_;
  const uint16_t kBranching_;
  const uint32_t kScaledInverseBranching_;

  Allocator* allocator_;

  Node* const head_;

  std::atomic<int> max_height_;

  inline int GetMaxHeight() const;

  int RandomHeight();

  Node* AllocateNode(const int32_t key, const int32_t value, const int height);

  bool Equal(const int32_t& a, const int32_t& b) const;

  bool LessThan(const int32_t& a, const int32_t& b) const;

  bool KeyIsAfterNode(const int32_t key, Node* n) const;

  bool KeyIsBeforeNode(const int32_t key, Node* n) const;

  Node* FindGreaterOrEqual(const int32_t key) const;

  template <bool prefetch_before>
  void FindSpliceForLevel(const int32_t key, Node* before, Node* after,
                          int level, Node** out_prev, Node** out_next);

  void RecomputeSpliceLevels(const int32_t key, Splice* splice,
                             int recompute_level);
};

struct SkipList::Splice {
  int height_ = 0;
  Node** prev_;
  Node** next_;
};

struct SkipList::Node {
  void StashHeight(const int height);

  int UnstashHeight() const;

  const int32_t* Key() const;

  const int32_t* Value() const;

  Node* Next(int n);

  void SetNext(int n, Node* x);

  bool CASNext(int n, Node* expected, Node* x);

  Node* NoBarrier_Next(int n);

  void NoBarrier_SetNext(int n, Node* x);

 private:
  std::atomic<Node*> next_[1];
};

}  // namespace MySkipList

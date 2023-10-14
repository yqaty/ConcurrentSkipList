#pragma once

#include <random>
namespace MySkipList {

extern std::random_device rd_;
extern std::mt19937 gen_;
extern std::uniform_int_distribution<int> dist_;

int rand_();

}  // namespace MySkipList

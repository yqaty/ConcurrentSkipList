#include "random.h"

#include <random>
namespace MySkipList {

std::random_device rd_;
std::mt19937 gen_(rd_());
std::uniform_int_distribution<int> dist_(0, (1 << 30) - 1);

int rand_() { return dist_(gen_); }

}  // namespace MySkipList
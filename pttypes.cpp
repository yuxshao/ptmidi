#include "pttypes.hpp"

std::ostream &operator<<(std::ostream &o, const Press &p) {
  return o << "(" << p.vel << ", " << p.length << ")";
}

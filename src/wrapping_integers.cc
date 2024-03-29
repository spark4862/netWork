#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
  return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
  uint64_t val = ((checkpoint >> 32) << 32);
  const uint32_t offset = raw_value_ - zero_point.raw_value_;
  val += offset;
  val = min({val - (1UL << 32), val, val + (1UL << 32)},
            [checkpoint](uint64_t a, uint64_t b) {
              return DIST(a, checkpoint) < DIST(b, checkpoint);
            });
  // 返回距离checkpoint最近的地址
  return val;
}

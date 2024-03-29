#include "byte_stream.hh"

#include <stdexcept>

// 重点：为了性能实现延迟更改

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity) {}

void Writer::push(string data) {
  if (available_capacity() == 0 || data.empty()) {
    return;
  }
  auto const n = min(available_capacity(), data.size());
  if (n < data.size()) {
    data = data.substr(0, n);
  }
  buffer_.push(move(data));
  bytes_buffered_ += n;
  bytes_pushed_ += n;
}

void Writer::close() { closed_ = true; }

void Writer::set_error() { error_ = true; }
bool Writer::is_closed() const { return closed_; }

uint64_t Writer::available_capacity() const {
  return capacity_ - bytes_buffered_;
}

uint64_t Writer::bytes_pushed() const { return bytes_pushed_; }

// 注意看辅助函数得到peek的定义
string_view Reader::peek() const {
  if (buffer_.empty()) {
    return {};
  }
  string_view ans = string_view{buffer_.front()};
  ans.remove_prefix(removed_prefix_);
  return ans;
}

bool Reader::is_finished() const { return closed_ && bytes_buffered_ == 0; }

bool Reader::has_error() const { return error_; }

void Reader::pop(uint64_t len) {
  auto n = min(len, bytes_buffered_);
  bytes_buffered_ -= n;
  bytes_popped_ += n;
  while (n > 0) {
    auto sz = buffer_.front().size() - removed_prefix_;
    if (n < sz) {
      removed_prefix_ += len;
      return;
    }
    removed_prefix_ = 0;
    buffer_.pop();
    n -= sz;
  }
}

uint64_t Reader::bytes_buffered() const { return bytes_buffered_; }

uint64_t Reader::bytes_popped() const { return bytes_popped_; }
#include "reassembler.hh"

using namespace std;

/*
可以使用维护颜色段的方法实现，如珂朵莉树
*/

void Reassembler::insert(uint64_t first_index, string data,
                         bool is_last_substring, Writer& output) {
  const uint64_t end_index = first_index + data.size();
  if (end_index < next_seq_num_) {
    return;
  }
  if (is_last_substring) {
    last_substring_end_index_ = end_index;
  }
  if (first_index > next_seq_num_) {
    auto [success, hint] = fit_string(data, first_index);
    if (!success) {
      return;
    }
    if (!fit_space(data, output)) {
      return;
    }
    bytes_pending_ += data.size();
    substrings_.emplace_hint(hint, first_index, std::move(data));

  } else {
    const auto max_space = min(space(output), end_index - next_seq_num_);

    data.erase(0, next_seq_num_ - first_index).erase(max_space);
    output.push(std::move(data));
    next_seq_num_ += max_space;

    scan_storage(output);
  }
  if (output.bytes_pushed() == last_substring_end_index_) {
    output.close();
  }
}

bool Reassembler::fit_space(std::string& data, const Writer& output) {
  if (data.size() > space(output)) {
    return false;
  }
  if (data.size() == space(output)) {
    data.pop_back();
  }
  return true;
}

uint64_t Reassembler::bytes_pending() const { return bytes_pending_; }

uint64_t Reassembler::space(const Writer& writer) const {
  return writer.available_capacity() - bytes_pending_;
}

std::pair<bool, MapIt_t> Reassembler::fit_string(std::string& data,
                                                 uint64_t& first_index) {
  if (substrings_.empty()) {
    return {true, substrings_.end()};
  }
  auto end_index = first_index + data.size();

  // Case 1: lower_bound - 1
  auto it = substrings_.lower_bound(first_index);
  if (it != substrings_.begin()) {
    it--;
    // if overlapping
    const auto end_index_it = end_index_of(it);
    if (end_index_it > first_index) {
      if (end_index_it >= end_index) {
        // this string is already contained by the string iterator points to
        return {false, {}};
      }
      // cut off the overlapping part
      data.erase(0, end_index_it - first_index);
      first_index = end_index_it;
    }
    // whether the first_index is updated or not,
    // previous string is still the lower bound of new string
    it++;
  }
  // the new string is the last string in reassembler storage ( no overlapping )
  if (it == substrings_.end()) {
    return {true, substrings_.end()};
  }

  // Case 2: lower_bound ( equal )
  if (it->first == first_index) {
    end_index = data.size() + first_index;
    if (end_index_of(it) >= end_index) {
      // this string is already contained by the string iterator points to
      return {false, {}};
    }
    // erase the string iterator points to and get next string
    it = erase_substring_by(it);
    if (it == substrings_.end()) {
      return {true, substrings_.end()};
    }
  }

  // Case 3: upper bound
  end_index = data.size() + first_index;
  while (it->first < end_index) {
    if (end_index_of(it) >= end_index) {
      end_index = it->first;
      data.erase(end_index - first_index);
      break;
    }
    it = erase_substring_by(it);
    if (it == substrings_.end()) {
      return {true, substrings_.end()};
    }
  }
  return {true, it};
}

uint64_t Reassembler::end_index_of(MapIt_t it) {
  return it->first + it->second.size();
}

void Reassembler::scan_storage(Writer& writer) {
  for (auto it = substrings_.begin();
       it != substrings_.end() && it->first <= next_seq_num_;) {
    auto end_index = end_index_of(it);
    if (end_index > next_seq_num_) {
      writer.push(std::move(it->second.erase(0, next_seq_num_ - it->first)));
      next_seq_num_ = end_index;
      // do not use erase_substring_by(), it->second is moved and thus its size
      // is changed
      bytes_pending_ -= end_index - it->first;
      it = substrings_.erase(it);
    } else {
      it = erase_substring_by(it);
    }
  }
}

MapIt_t Reassembler::erase_substring_by(MapIt_t it) {
  bytes_pending_ -= it->second.size();
  return substrings_.erase(it);
}
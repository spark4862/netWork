#include "tcp_sender.hh"

#include <random>

#include "tcp_config.hh"

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender(uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn)
    : isn_(fixed_isn.value_or(Wrap32{random_device()()})),
      timer_(make_unique<Timer>(initial_RTO_ms)) {}

optional<TCPSenderMessage> TCPSender::maybe_send() {
  if (retransmit_flag_ && has_outstanding_segment()) {
    timer_->run();
    retransmit_flag_ = false;
    return segments_.front();
  }
  if (has_cached_segment()) {
    timer_->run();
    return segments_[next_segment_++];
  }
  return {};
}

void TCPSender::push(Reader& outbound_stream) {
  uint64_t window_size = remaining_window_size_;
  if (window_size == 0 && can_use_magic_) {
    can_use_magic_ = false;
    window_size = 1;
  }
  while (window_size > 0) {
    TCPSenderMessage msg{};

    // Deal with SYN
    if (absolute_seqno_ == 0) {
      window_size -= 1;
      msg.SYN = true;
    }

    // Deal with payload
    uint64_t payload_size = min({TCPConfig::MAX_PAYLOAD_SIZE, window_size,
                                 outbound_stream.bytes_buffered()});
    if (payload_size > 0) {
      string payload{};
      read(outbound_stream, payload_size, payload);
      window_size -= payload.size();
      msg.payload = std::move(payload);
    }

    // Deal with FIN
    if (window_size > 0 && outbound_stream.is_finished() &&
        !pre_segment_has_FIN_) {
      msg.FIN = true;
      pre_segment_has_FIN_ = true;
      window_size--;
    }
    if (msg.sequence_length() == 0) {
      return;
    }
    if (msg.FIN && !available_to_send_FIN_) {
      pre_segment_has_FIN_ = false;
      return;
    }

    msg.seqno = Wrap32::wrap(absolute_seqno_, isn_);
    absolute_seqno_ += msg.sequence_length();
    sequence_numbers_in_flight_ += msg.sequence_length();
    segments_.push_back(std::move(msg));

    remaining_window_size_ = window_size;
  }
}

TCPSenderMessage TCPSender::send_empty_message() const {
  return {Wrap32::wrap(absolute_seqno_, isn_)};
}

void TCPSender::receive(const TCPReceiverMessage& msg) {
  uint64_t current_unwraped_ackno = 0;

  // The msg's ackno field is possibly empty if the receiver hasn't received the
  // isn yet.
  if (msg.ackno.has_value()) {
    current_unwraped_ackno = msg.ackno.value().unwrap(isn_, absolute_seqno_);
  }

  // Receive ack for segment not yet sent
  if (current_unwraped_ackno > absolute_seqno_) {
    return;
  }

  // Don't add FIN if this would make the segment exceed the receiver's window
  available_to_send_FIN_ =
      msg.window_size + current_unwraped_ackno > absolute_seqno_;
  if (msg.window_size == 0) {
    available_to_send_FIN_ = current_unwraped_ackno >= absolute_seqno_;
    can_use_magic_ = true;
  }

  //
  remaining_window_size_ =
      current_unwraped_ackno + msg.window_size - absolute_seqno_;
  window_is_zero_ = remaining_window_size_ == 0;

  if (pre_unwarped_ackno_ < current_unwraped_ackno) {
    receive_new_ack(current_unwraped_ackno);
  }
}

void TCPSender::receive_new_ack(uint64_t new_unwraped_ackno) {
  timer_->set_RTO_by_factor(0);
  if (has_outstanding_segment()) {
    timer_->restart();
  }
  consecutive_retransmissions_ = 0;
  pre_unwarped_ackno_ = new_unwraped_ackno;
  remove_acked_segment(new_unwraped_ackno);
}

void TCPSender::remove_acked_segment(uint64_t unwraped_ackno) {
  while (has_outstanding_segment()) {
    auto it = segments_.cbegin();
    const uint64_t end_absolute_seqno =
        it->seqno.unwrap(isn_, absolute_seqno_) + it->sequence_length();
    if (unwraped_ackno < end_absolute_seqno) {
      // This segment hasn't been fully acked yet.
      return;
    }
    sequence_numbers_in_flight_ -= it->sequence_length();
    segments_.pop_front();
    next_segment_ -= 1;
  }
}

void TCPSender::tick(uint64_t ms_since_last_tick) {
  if (!has_outstanding_segment() && !has_cached_segment()) {
    timer_->stop();
    return;
  }
  timer_->elapse(ms_since_last_tick);
  if (timer_->expired()) {
    retransmit_flag_ = true;
    if (!window_is_zero_) {
      consecutive_retransmissions_ += 1;
      timer_->set_RTO_by_factor(2);
    } else {
      timer_->set_RTO_by_factor(0);
    }
    timer_->restart();
  }
}

bool TCPSender::has_outstanding_segment() const { return next_segment_ != 0; }

bool TCPSender::has_cached_segment() const {
  return next_segment_ != segments_.size();
}

// for use in testing
uint64_t TCPSender::sequence_numbers_in_flight() const {
  return sequence_numbers_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const {
  return consecutive_retransmissions_;
}

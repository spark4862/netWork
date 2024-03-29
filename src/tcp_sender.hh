#pragma once

#include <memory>

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class Timer {
 private:
  bool is_running_ = false;
  uint64_t initial_RTO_ms_;
  uint64_t time_elapsed_ = 0;
  uint64_t current_RTO_ms_;

 public:
  explicit Timer(uint64_t initial_RTO_ms)
      : initial_RTO_ms_(initial_RTO_ms), current_RTO_ms_(initial_RTO_ms) {}

  void run() { is_running_ = true; }

  void elapse(uint64_t time) {
    if (is_running()) {
      time_elapsed_ += time;
    }
    if (current_RTO_ms_ <= time_elapsed_) {
      expire();
    }
  }

  // Multiple current_RTO_ms by a factor greater or equal 0.
  // If factor == 0, reset RTO to initial_RTO_ms.
  void set_RTO_by_factor(uint8_t factor) {
    if (factor == 0) {
      current_RTO_ms_ = initial_RTO_ms_;
    } else {
      current_RTO_ms_ *= factor;
    }
  }

  void stop() { is_running_ = false; }

  void expire() {
    is_running_ = false;
    time_elapsed_ = 0;
  }

  void restart() {
    is_running_ = true;
    time_elapsed_ = 0;
  }

  bool expired() const { return !is_running_ && time_elapsed_ == 0; }

  bool is_running() const { return is_running_; }
};

class TCPSender {
  Wrap32 isn_;
  std::unique_ptr<Timer> timer_;

  // send msg with 1 byte when windowSize equals 0 for 1 time
  bool can_use_magic_ = false;
  bool window_is_zero_ = false;
  // init this as 1 to send syn
  uint16_t remaining_window_size_ = 1;

  // When timer is expired, set this flag to true.
  // Set this flag to false after retransmitting the segment.
  bool retransmit_flag_ = false;

  bool pre_segment_has_FIN_ = false;
  bool available_to_send_FIN_ = false;

  // absSeq of seqno
  uint64_t absolute_seqno_ = 0;
  uint64_t pre_unwarped_ackno_ = 0;

  // nums of segment sent but not acked
  size_t next_segment_ = 0;
  // segments not acked and not sent
  std::deque<TCPSenderMessage> segments_{};

  uint64_t sequence_numbers_in_flight_ = 0;
  uint64_t consecutive_retransmissions_ = 0;

 public:
  /* Construct TCP sender with given default Retransmission Timeout and possible
   * ISN */
  TCPSender(uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn);

  /* Push bytes from the outbound stream */
  void push(Reader& outbound_stream);

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive(const TCPReceiverMessage& msg);

  /* Time has passed by the given # of milliseconds since the last time the
   * tick() method was called. */
  void tick(uint64_t ms_since_last_tick);

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight()
      const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions()
      const;  // How many consecutive *re*transmissions have happened?

 private:
  void remove_acked_segment(uint64_t current_unwraped_ackno);
  void receive_new_ack(uint64_t new_unwraped_ackno);
  bool has_outstanding_segment() const;  // sent but unacked
  bool has_cached_segment() const;       // not yet send but usable
};

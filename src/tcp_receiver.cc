#include "tcp_receiver.hh"

using namespace std;

/*
 * The message can have both the FIN bit and the SYN bit set, and possibly carry
 * payload. Reset the isn_ if receive a new SYN (i.e. a SYN message with new
 * isn_). Do nothing if receive a old SYN (i.e. a SYN message with current
 * isn_). It is still possible for a segment to arrive after the receiver
 * receives the FIN segment.
 *
 * Hint:
 *  The bytes_pushed() method of inbound_stream returns the next stream index it
 * expects. The bytes_pushed() + 1 represents the next absolute sequence index
 * it expects.
 */
void TCPReceiver::receive(TCPSenderMessage message, Reassembler& reassembler,
                          Writer& inbound_stream) {
  if (isn_.has_value() && message.seqno == isn_) {
    return;
  }
  if (message.SYN) {
    isn_ = message.seqno;
    ackno_ = isn_.value() + message.sequence_length();
    reassembler.insert(0, message.payload, message.FIN, inbound_stream);
  } else if (isn_.has_value()) {
    // absolute seqno to streamindex, so needed to minus 1
    reassembler.insert(
        message.seqno.unwrap(isn_.value(), inbound_stream.bytes_pushed()) - 1,
        message.payload, message.FIN, inbound_stream);
    // streamindex to absolute seqno, so needed to plus 1
    ackno_ = Wrap32::wrap(inbound_stream.bytes_pushed() + 1, isn_.value());
  }
  if (message.FIN && isn_.has_value()) {
    FIN_seqno_ = message.seqno + (message.sequence_length() - 1);
  }
  if (FIN_seqno_.has_value() && FIN_seqno_ == ackno_) {
    ackno_ = ackno_ + 1;
    inbound_stream.close();
  }
}

TCPReceiverMessage TCPReceiver::send(const Writer& inbound_stream) const {
  const uint16_t window_size =
      min(MAX_RWND_SIZE, inbound_stream.available_capacity());
  if (!isn_.has_value()) {
    return {{}, window_size};
  }
  return {ackno_, window_size};
}

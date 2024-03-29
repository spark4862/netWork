#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the
// interface ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress& ethernet_address,
                                   const Address& ip_address)
    : ethernet_address_(ethernet_address), ip_address_(ip_address) {
  cerr << "DEBUG: Network interface has Ethernet address "
       << to_string(ethernet_address_) << " and IP address " << ip_address.ip()
       << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router
// or default gateway, but may also be another host if directly connected to the
// same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address)
// by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram& dgram,
                                     const Address& next_hop) {
  if (const auto it = arp_cache_.find(next_hop.ipv4_numeric());
      it != arp_cache_.end()) {
    // The destination Ethernet address is already known
    frames_.emplace(make_frame(it->second.ethernet_address_,
                               EthernetHeader::TYPE_IPv4, serialize(dgram)));
  } else {
    if (const auto it_dgram = dgrams_.find(next_hop.ipv4_numeric());
        it_dgram != dgrams_.end()) {
      // Already sent ARP request about the datagram's dest ip address
      return;
    }
    frames_.emplace(
        make_frame(ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP,
                   serialize(make_arp(ARPMessage::OPCODE_REQUEST, {},
                                      next_hop.ipv4_numeric()))));
    // Queue the datagram
    dgrams_.emplace(next_hop.ipv4_numeric(), DatagramWithTimer{dgram, 0});
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(
    const EthernetFrame& frame) {
  if (frame.header.type == EthernetHeader::TYPE_IPv4 &&
      frame.header.dst == ethernet_address_) {
    InternetDatagram dgram;
    // Receive ipv4 datagram, send it to up stack
    if (parse(dgram, frame.payload)) {
      return dgram;
    }
    return {};
  }
  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp;
    if (parse(arp, frame.payload)) {
      // Remember the mapping of sender
      arp_cache_.emplace(
          arp.sender_ip_address,
          EthernetAddressWithTimer{arp.sender_ethernet_address, 0});
      // Send cached dgram
      if (const auto it = dgrams_.find(arp.sender_ip_address);
          it != dgrams_.end()) {
        frames_.emplace(make_frame(arp.sender_ethernet_address,
                                   EthernetHeader::TYPE_IPv4,
                                   serialize(it->second.dgram_)));
        dgrams_.erase(it);
      }
      // Generate arp reply
      if (arp.target_ip_address == ip_address_.ipv4_numeric() &&
          arp.opcode == ARPMessage::OPCODE_REQUEST) {
        frames_.emplace(
            make_frame(arp.sender_ethernet_address, EthernetHeader::TYPE_ARP,
                       serialize(make_arp(ARPMessage::OPCODE_REPLY,
                                          arp.sender_ethernet_address,
                                          arp.sender_ip_address))));
      }
    }
  }
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this
// method
void NetworkInterface::tick(size_t ms_since_last_tick) {
  for (auto it = arp_cache_.begin(); it != arp_cache_.end();) {
    it->second.age_ += ms_since_last_tick;
    if (it->second.age_ >= MAX_LIFE_TIME) {
      it = arp_cache_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto& dgram : dgrams_) {
    dgram.second.time_ += ms_since_last_tick;
    if (dgram.second.time_ >= ARP_MESSAGE_TIMEOUT) {
      frames_.emplace(make_frame(
          ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP,
          serialize(make_arp(ARPMessage::OPCODE_REQUEST, {}, dgram.first))));
      dgram.second.time_ -= ARP_MESSAGE_TIMEOUT;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send() {
  if (!frames_.empty()) {
    EthernetFrame frame = std::move(frames_.front());
    frames_.pop();
    return frame;
  }
  return {};
}

ARPMessage NetworkInterface::make_arp(
    uint16_t opcode, EthernetAddress target_ethernet_address,
    uint32_t target_ip_address_numeric) const {
  ARPMessage arp;
  arp.opcode = opcode;
  arp.sender_ethernet_address = ethernet_address_;
  arp.sender_ip_address = ip_address_.ipv4_numeric();
  arp.target_ethernet_address = target_ethernet_address;
  arp.target_ip_address = target_ip_address_numeric;
  return arp;
}

EthernetFrame NetworkInterface::make_frame(const EthernetAddress& dst,
                                           uint16_t type,
                                           vector<Buffer> payload) const {
  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.dst = dst;
  frame.header.type = type;
  frame.payload = std::move(payload);
  return frame;
}

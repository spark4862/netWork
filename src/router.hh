#pragma once

#include <optional>
#include <queue>
#include <vector>

#include "network_interface.hh"

// A wrapper for NetworkInterface that makes the host-side
// interface asynchronous: instead of returning received datagrams
// immediately (from the `recv_frame` method), it stores them for
// later retrieval. Otherwise, behaves identically to the underlying
// implementation of NetworkInterface.
class AsyncNetworkInterface : public NetworkInterface {
  std::queue<InternetDatagram> datagrams_in_{};

 public:
  using NetworkInterface::NetworkInterface;

  // Construct from a NetworkInterface
  explicit AsyncNetworkInterface(NetworkInterface&& interface)
      : NetworkInterface(interface) {}

  // \brief Receives and Ethernet frame and responds appropriately.

  // - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval
  // by the owner.
  // - If type is ARP request, learn a mapping from the "sender" fields, and
  // send an ARP reply.
  // - If type is ARP reply, learn a mapping from the "target" fields.
  //
  // \param[in] frame the incoming Ethernet frame
  void recv_frame(const EthernetFrame& frame) {
    auto optional_dgram = NetworkInterface::recv_frame(frame);
    if (optional_dgram.has_value()) {
      datagrams_in_.push(std::move(optional_dgram.value()));
    }
  };

  // Access queue of Internet datagrams that have been received
  std::optional<InternetDatagram> maybe_receive() {
    if (datagrams_in_.empty()) {
      return {};
    }

    InternetDatagram datagram = std::move(datagrams_in_.front());
    datagrams_in_.pop();
    return datagram;
  }
};

// A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router {
 private:
  // The router's collection of network interfaces
  std::vector<AsyncNetworkInterface> interfaces_{};

  // For routing table
  struct Route {
    static constexpr uint8_t MAX_IP_ADDR_LEN = 32;

    uint32_t route_prefix_;
    uint8_t prefix_length_;
    std::optional<Address> next_hop_;
    size_t interface_num_;

    // Marking route_prefix const to avoid clang-tidy warning
    explicit Route(const uint32_t route_prefix, uint8_t prefix_length,
                   std::optional<Address> next_hop, size_t interface_num)
        : route_prefix_(route_prefix),
          prefix_length_(prefix_length),
          next_hop_(next_hop),
          interface_num_(interface_num) {}

    bool operator<(const Route& rhs) const {
      return this->prefix_length_ < rhs.prefix_length_;
    }

    bool match(uint32_t other_ip_address) const {
      uint64_t res = route_prefix_ ^ other_ip_address;
      uint8_t offset = MAX_IP_ADDR_LEN - prefix_length_;
      return (res >> offset) == 0;
    }
  };

  std::vector<Route> routes_{};

 public:
  // Add an interface to the router
  // interface: an already-constructed network interface
  // returns the index of the interface after it has been added to the router
  size_t add_interface(AsyncNetworkInterface&& interface) {
    interfaces_.push_back(std::move(interface));
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  AsyncNetworkInterface& interface(size_t N) { return interfaces_.at(N); }

  // Add a route (a forwarding rule)
  void add_route(uint32_t route_prefix, uint8_t prefix_length,
                 std::optional<Address> next_hop, size_t interface_num);

  // Route packets between the interfaces. For each interface, use the
  // maybe_receive() method to consume every incoming datagram and
  // send it on one of interfaces to the correct next hop. The router
  // chooses the outbound interface and next-hop as specified by the
  // route with the longest prefix_length that matches the datagram's
  // destination address.
  void route();
};

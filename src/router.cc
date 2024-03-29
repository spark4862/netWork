#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's
// destination address against prefix_length: For this route to be applicable,
// how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the
//    datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is
// directly attached to the router (in
//    which case, the next hop address should be the datagram's final
//    destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix, const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
  cerr << "DEBUG: adding route "
       << Address::from_ipv4_numeric(route_prefix).ip() << "/"
       << static_cast<int>(prefix_length) << " => "
       << (next_hop.has_value() ? next_hop->ip() : "(direct)")
       << " on interface " << interface_num << "\n";

  routes_.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

void Router::route() {
  for (auto& inf : interfaces_) {
    auto optional_dgram = inf.maybe_receive();
    while (optional_dgram.has_value()) {
      InternetDatagram dgram = std::move(optional_dgram.value());
      size_t best = SIZE_MAX;
      for (size_t i = 0; i < routes_.size(); ++i) {
        if (routes_[i].match(dgram.header.dst) &&
            (best == SIZE_MAX || routes_[best] < routes_[i])) {
          best = i;
        }
      }
      if (best != SIZE_MAX && dgram.header.ttl > 1) {
        dgram.header.ttl--;
        // dont forget to recompute checkSum
        dgram.header.compute_checksum();
        Address next_hop = routes_[best].next_hop_.value_or(
            Address::from_ipv4_numeric(dgram.header.dst));
        interface(routes_[best].interface_num_).send_datagram(dgram, next_hop);
      }
      optional_dgram = inf.maybe_receive();
    }
  }
}

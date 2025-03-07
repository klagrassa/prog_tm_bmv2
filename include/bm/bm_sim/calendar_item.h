#ifndef BM_BM_SIM_CALENDAR_ITEM_H_
#define BM_BM_SIM_CALENDAR_ITEM_H_

#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/phv.h>
#ifdef BM_ENABLE_TM_DEBUG
#include <bm/bm_sim/logger.h>
#endif

#include <memory>   // std::shared_ptr
#include <utility>  // std::pair

namespace bm {

/**
 * @brief CalendarItem class
 * This class is used to represent an item in the calendar, which is the packet
 * descriptor.
 */
class CalendarItem {
 public:
  CalendarItem(std::shared_ptr<bm::Packet> pkt_ptr)
      : rank(0, 0),
        packet_id(0u),
        packet_ptr(pkt_ptr),
        egress_port(0),
        packet_size(0),
        priority(0),
        dscp(0),
        color(0),
        vlan_id(0),
        sport(0),
        dport(0) {
    packet_id = pkt_ptr->get_packet_id();
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("Packet for CalItem egress port : {}",
                pkt_ptr->get_egress_port());
#endif
    egress_port = pkt_ptr->get_egress_port();
    packet_size = pkt_ptr->get_data_size();
    const auto* phv = pkt_ptr->get_phv();
    if (phv->has_field("standard_metadata.egress_port")) {
      egress_port = phv->get_field("standard_metadata.egress_port").get_uint();
    }
    if (phv->has_field("intrinsic_metadata.packet_length")) {
      packet_size =
          phv->get_field("intrinsic_metadata.packet_length").get_uint();
    }
    if (phv->has_field("intrinsic_metadata.priority")) {
      priority = phv->get_field("intrinsic_metadata.priority").get_uint();
    }
    if (phv->has_field("ipv4.diffserv")) {
      dscp = phv->get_field("ipv4.diffserv").get_uint();
    }
    if (phv->has_field("scalars.metadata.color")) {
      color = phv->get_field("scalars.metadata.color").get_uint();
    }
  }

  // Getters
  std::pair<int, int> get_rank() const { return rank; }
  std::uint32_t get_packet_id() const { return packet_id; }
  std::uint32_t get_egress_port() const { return egress_port; }
  size_t get_packet_size() const { return packet_size; }
  std::uint8_t get_priority() const { return priority; }
  std::uint8_t get_dscp() const { return dscp; }
  std::uint8_t get_color() const { return color; }
  std::uint16_t get_vlan_id() const { return vlan_id; }
  std::uint8_t get_sport() const { return sport; }
  std::uint8_t get_dport() const { return dport; }
  bm::Packet* get_packet_ptr() const { return packet_ptr.get(); }

  // Setters
  void set_rank(const std::pair<int, int>& r) { rank = r; }
  void set_packet_id(std::uint32_t id) { packet_id = id; }
  void set_egress_port(std::uint32_t port) { egress_port = port; }
  void set_packet_size(size_t size) { packet_size = size; }
  void set_priority(std::uint8_t prio) { priority = prio; }
  void set_dscp(std::uint8_t d) { dscp = d; }
  void set_color(std::uint8_t c) { color = c; }
  void set_vlan_id(std::uint16_t vlan) { vlan_id = vlan; }
  void set_sport(std::uint8_t s) { sport = s; }
  void set_dport(std::uint8_t d) { dport = d; }

 private:
  std::pair<int, int> rank;
  std::uint32_t packet_id;
  std::shared_ptr<bm::Packet> packet_ptr;

  /* P4 User accessible data */
  std::uint32_t egress_port;
  size_t packet_size;
  std::uint8_t priority;
  std::uint8_t dscp;
  std::uint8_t color;
  std::uint16_t vlan_id;
  std::uint8_t sport;
  std::uint8_t dport;
};

}  // namespace bm

#endif  // BM_BM_SIM_CALENDAR_ITEM_H_
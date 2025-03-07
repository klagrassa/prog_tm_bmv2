#include "interface_tm.h"

#include <bm/bm_sim/logger.h>

namespace bm {

BM_REGISTER_EXTERN(TrafficManagerInterface)

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, get_scheduler_parameter,
                          const Data &, const Data &, Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, get_size_of_parameter,
                          const Data &, Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, write_to_reg, const Data &,
                          const Data &, const Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, read_from_reg, const Data &,
                          const Data &, Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, set_rank, const Data &,
                          const Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, set_predicate, const Data &,
                          const Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, get_lowest_priority_for_day,
                          const Data &, Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, get_lowest_priority, Data &,
                          Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, set_field, const Data &,
                          const Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, get_field, const Data &,
                          Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, get_rank, const Data &,
                          Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, has_packets, const Data &,
                          Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, find_next_non_empty_day,
                          const Data &, const Data &, Data &);

BM_REGISTER_EXTERN_METHOD(TrafficManagerInterface, find_non_empty_day,
                          const Data &, const Data &, Data &);

/// @brief Get a scheduler parameter
/// @param param_index index of the parameter
/// @param value value to set (output of the function)
/// @param index index of the RegisterArray
void TrafficManagerInterface::get_scheduler_parameter(const Data &param_index,
                                                      const Data &reg_index,
                                                      Data &value) {
  // Check if the parameter exists
  if (param_index.get<size_t>() >= scheduler_params.size()) {
    bm::Logger::get()->error("Scheduler parameter index {} out of bounds",
                             param_index.get<size_t>());
    return;
  }
  auto it = scheduler_params.begin();
  std::advance(it, param_index.get<size_t>());
  value.set(it->second->at(reg_index.get<size_t>()).get<size_t>());
}

/// @brief Return the size of a scheduler parameter
/// @param param_index index of the parameter
/// @param value value to set (output of the function)
void TrafficManagerInterface::get_size_of_parameter(const Data &param_index,
                                                    Data &value) {
  if (this->scheduler_params.find(param_index.get<size_t>()) ==
      scheduler_params.end()) {
    bm::Logger::get()->error("Scheduler parameter {} does not exist",
                             param_index);
    return;
  }
  value.set(Data{this->scheduler_params[param_index.get<size_t>()]->size()});
  BMLOG_DEBUG("Size of scheduler parameter {} is {}", param_index.get<size_t>(),
              value.get<size_t>());
}

/// @brief Write to a general purpose register
/// @param reg_number the index of the register
/// @param value value to write
/// @param idx index of the RegisterArray
void TrafficManagerInterface::write_to_reg(const Data &reg_number,
                                           const Data &idx, const Data &value) {
  // Check if the parameter exists
  if (gen_purpose_reg.find(reg_number.get<size_t>()) == gen_purpose_reg.end()) {
    bm::Logger::get()->error("Register index {} out of bounds",
                             reg_number.get<size_t>());
    return;
  }
  gen_purpose_reg[reg_number.get<size_t>()]
      ->at(idx.get<size_t>())
      .set(value.get<size_t>());

  BMLOG_DEBUG("Wrote {} to register {} at index {}", value.get<size_t>(),
              reg_number.get<size_t>(), idx.get<size_t>());
}

/// @brief Read from a general purpose register
/// @param reg_number the index of the register
/// @param idx index of the RegisterArray
/// @param value value to set (output of the function)
void TrafficManagerInterface::read_from_reg(const Data &reg_number,
                                            const Data &idx, Data &value) {
  // Check if the parameter exists
  if (gen_purpose_reg.find(reg_number.get<size_t>()) == gen_purpose_reg.end()) {
    bm::Logger::get()->error("Register index {} out of bounds",
                             reg_number.get<size_t>());
    return;
  }
  value.set(gen_purpose_reg[reg_number.get<size_t>()]
                ->at(idx.get<size_t>())
                .get<size_t>());

  BMLOG_DEBUG("Read {} from register {} at index {}", value.get<size_t>(),
              reg_number.get<size_t>(), idx.get<size_t>());
}

/// @brief Thread safe modifier to the rank register
/// @param value
void TrafficManagerInterface::set_rank(const Data &day, const Data &time) {
  rank->at(0).set(day.get<size_t>());
  rank->at(1).set(time.get<size_t>());
  // BMLOG_DEBUG("Set rank {}", rank->at(1).get<size_t>());
}

void TrafficManagerInterface::get_rank(const Data &, Data &value) {
  // read the value of the rank register at day index
  value.set(get_rank().second);
}

/// @brief Thraed safe modifier to the predicate register
/// @param value
void TrafficManagerInterface::set_predicate(const Data &day, const Data &time) {
  BMLOG_DEBUG("Setting predicate to day {} and time {}", day.get<size_t>(),
              time.get<size_t>());
  predicate->at(0).set(day.get<size_t>());
  predicate->at(1).set(time.get<size_t>());
}

/**
 * @brief Return the lowest priority packet in the calendar queue
 * Simplest case
 */
void TrafficManagerInterface::get_lowest_priority(Data &day, Data &value) {
  // Locks to be done ?
  std::shared_ptr<CalendarItem> cal_item = owner->get_lowest();

  if (cal_item == nullptr) {
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("No packet found in calendar queue, returning 0 rank");
#endif
    day.set(0);
    value.set(0);
    return;
  }

  day.set(cal_item->get_rank().first);
  value.set(cal_item->get_rank().second);
}

/**
 * @brief Return the lowest priority packet in the calendar queue for a given
 * day More complex case
 */
void TrafficManagerInterface::get_lowest_priority_for_day(const Data &day,
                                                          Data &value) {
  // Locks to be done ?
  std::shared_ptr<CalendarItem> cal_item =
      owner->get_lowest_for_day(day.get<int>());

  // TODO opti via returning a null value for the predicate ? letting P4 code
  // handling it
  if (cal_item == nullptr) {
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("No packet found in calendar queue, returning 0 rank");
#endif
    value.set(0);
    return;
  }

  value = Data{cal_item->get_rank().second};
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("LOWEST PRIORITY PACKET IS : {} from calendar queue",
              value.get<int>());
#endif

  // Load info to registers from the fields of CItem
  // for (auto field : cal_item->get_fields()) {
  //   packet_informations[field.first]->at(0).set(field.second);
  // }
}

// Safe access to the packet fields for these 2 ?
void TrafficManagerInterface::set_field(const Data &field_index,
                                        const Data &value) {
  this->packet_informations[field_index.get<size_t>()]->at(0).set(
      value.get<size_t>());
}

void TrafficManagerInterface::get_field(const Data &field_index, Data &value) {
  value.set(this->packet_informations[field_index.get<size_t>()]
                ->at(0)
                .get<size_t>());
}

// ----------------- END OF REGISTERED FUNCTIONS -----------------

/*
 * NON REGISTERED FUNCTIONS
 */

size_t TrafficManagerInterface::get_field(int field_index) {
  // Packets info is 2D array, all values in stored in the first row
  return packet_informations[field_index]->at(0).get<size_t>();
}

/// @brief Add a scheduler parameter
/// @param name Name of the parameter
/// @param value vector of values to initialize the RegisterArray
/// @param id id of the RegisterArray (not meaningful as of now)
/// @param size number of registers in the RegisterArray
/// @param bitwidth size of each register in bits
void TrafficManagerInterface::add_scheduler_parameter(const int &param_index,
                                                      std::vector<int> value,
                                                      p4object_id_t id,
                                                      size_t size,
                                                      int bitwidth) {
  if (scheduler_params.find(param_index) != scheduler_params.end()) {
    bm::Logger::get()->error("Scheduler parameter {} already exists",
                             param_index);
    return;
  }
  scheduler_params[param_index] = std::make_unique<bm::RegisterArray>(
      "scheduler_param_" + std::to_string(param_index), id, size, bitwidth);

  for (size_t i = 0; i < value.size(); i++) {
    scheduler_params[param_index]->at(i).set(value[i]);
  }
}

int TrafficManagerInterface::get_content_reg(int reg_index, int idx) {
  return gen_purpose_reg[reg_index]->at(idx).get<int>();
}

void TrafficManagerInterface::has_packets(const Data &day, Data &is_empty) {
  int day_idx = day.get<int>();

  bool empty = true;
  empty = !owner->has_packets_for_day(day_idx);
  BMLOG_DEBUG("Day {} is empty: {}", day_idx, empty);
  is_empty.set(empty);
}

void TrafficManagerInterface::find_next_non_empty_day(
    const Data &day, const Data &max_search_limit, Data &next_day) {
  int current_day = day.get<int>();
  int found_day = -1;
  // Use an arbitrary upper limit to avoid infinite loops.
  const int max_search = current_day + max_search_limit.get<int>();
  for (int d = current_day + 1; d < max_search; ++d) {
    if (owner->has_packets_for_day(d)) {
      found_day = d;
      break;
    }
  }
  if (found_day < 0) {
    BMLOG_DEBUG("No non-empty day found within the limit");
    // No non-empty day found within the limit; fall back to the current day.
    next_day.set(current_day);
  } else {
    BMLOG_DEBUG("Found non-empty day {}", found_day);
    next_day.set(found_day);
  }
}

void TrafficManagerInterface::find_non_empty_day(const Data &day_start,
                                                 const Data &max_search_limit,
                                                 Data &next_day) {
  int current_day = day_start.get<int>();
  int found_day = -1;
  // Use an arbitrary upper limit to avoid infinite loops.
  const int max_search = current_day + max_search_limit.get<int>();
  for (int d = current_day; d < max_search; ++d) {
    if (owner->has_packets_for_day(d)) {
      found_day = d;
      BMLOG_DEBUG("Found non-empty day {}", found_day);
      break;
    }
  }
  if (found_day < 0) {
    BMLOG_DEBUG("No non-empty day found within the limit");
    // No non-empty day found within the limit; fall back to the current day.
    next_day.set(current_day);
  } else {
    BMLOG_DEBUG("Found non-empty day {}", found_day);
    next_day.set(found_day);
  }
}

}  // namespace bm

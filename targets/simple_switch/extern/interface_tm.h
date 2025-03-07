#ifndef SIMPLE_SWITCH_EXTERN_INTERFACE_TM_H_
#define SIMPLE_SWITCH_EXTERN_INTERFACE_TM_H_

#include <bm/bm_sim/data.h>    // bm::Data
#include <bm/bm_sim/extern.h>  // bm::ExternType
#include <bm/bm_sim/logger.h>
#include <bm/bm_sim/node.h>
#include <bm/bm_sim/stateful.h>  // bm::RegisterArray
#include <bm/config.h>

#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#define MAX_NB_GP_REG 32
#define MAX_NB_SCHED_PARAM 32
#define MAX_SIZE_SCHED_PARAM_ARRAY 16
#define MAX_SIZE_GP_REG_ARRAY 16
#define BITWIDTH 32

namespace bm {
class Node;

/**
 * @brief TrafficManagerInterface class.
 * Used only for providing a P4 interface to the TrafficManager class.
 * Prototype use only.
 * Gives an access to registers and scheduler parameters.
 */
class TrafficManagerInterface : public bm::ExternType {
 public:
  BM_EXTERN_ATTRIBUTES {
    // Find a way to get the constructor call in C++
    // This registration is only for the P4 constructor call
  }

  void init() override {  // Attributes
    // Single register arrays
    this->rank = std::make_unique<bm::RegisterArray>("rank", 0, 2, BITWIDTH);
    this->predicate =
        std::make_unique<bm::RegisterArray>("predicate", 0, 2, BITWIDTH);

    this->reset();
  }

  void reset() {
    this->scheduler_params.clear();

    // Allocation of the general purpose registers
    for (size_t i = 0; i < MAX_NB_GP_REG; i++) {
      gen_purpose_reg[i] = std::make_unique<bm::RegisterArray>(
          "gen_purpose_reg_" + std::to_string(i), 0, MAX_SIZE_GP_REG_ARRAY,
          BITWIDTH);
    }

    // Initialization of the packet fields, needed for predicate
    for (size_t i = 0; i < MAX_NB_SCHED_PARAM; i++) {
      // 1 register for each field (MAX_NB_SCHED_PARAM fields per packet)
      packet_informations[i] = std::make_unique<bm::RegisterArray>(
          "packet_field_" + std::to_string(i), 0, 1, BITWIDTH);
    }
  }

  /*
   * REGISTERED FUNCTIONS
   */

  /// @brief Get a scheduler parameter
  /// @param param_index index of the parameter
  /// @param value value to set (output of the function)
  /// @param index index of the RegisterArray
  void get_scheduler_parameter(const bm::Data &param_index,
                               const bm::Data &reg_index, bm::Data &value);

  /// @brief Return the size of a scheduler parameter
  /// @param param_index index of the parameter
  /// @param value value to set (output of the function)
  void get_size_of_parameter(const bm::Data &param_index, bm::Data &value);

  /// @brief Write to a general purpose register
  /// @param reg_number the index of the register
  /// @param value value to write
  /// @param idx index of the RegisterArray
  void write_to_reg(const bm::Data &reg_number, const bm::Data &idx,
                    const bm::Data &value);

  /// @brief Read from a general purpose register
  /// @param reg_number the index of the register
  /// @param idx index of the RegisterArray
  /// @param value value to set (output of the function)
  void read_from_reg(const bm::Data &reg_number, const bm::Data &idx,
                     bm::Data &value);

  /// @brief Thread safe modifier to the rank register
  /// @param value
  void set_rank(const bm::Data &day, const bm::Data &time);

  void get_rank(const Data &day, Data &value);

  /// @brief Thraed safe modifier to the predicate register
  /// @param value
  void set_predicate(const bm::Data &day, const bm::Data &time);

  /// @brief Return the lowest priority packet in the calendar queue
  /// @param current_day day to search
  /// @param value output of the function
  void get_lowest_priority_for_day(const bm::Data &current_day,
                                   bm::Data &value);

  void get_lowest_priority(Data &day, Data &value);

  void set_field(const bm::Data &field_index, const bm::Data &value);

  void get_field(const bm::Data &field_index, bm::Data &value);

  // ----------------- END OF REGISTERED FUNCTIONS -----------------

  /*
   * NON REGISTERED FUNCTIONS (must be defined here)
   */

  void set_owner(bm::Node *p) { this->owner = p; }

  /// @brief Return the rank register. Thread safe access
  /// @return the value of the rank register
  std::pair<int, int> get_rank() const {
    // auto lock = rank->unique_lock();
    int day = rank->at(0).get<int>();
    int time = rank->at(1).get<int>();
    // rank->unlock(lock);
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("Read day:{}, time:{} from rank register", day, time);
#endif
    return std::make_pair(day, time);
  }

  /// @brief Add a scheduler parameter
  /// @param name Name of the parameter
  /// @param value vector of values to initialize the RegisterArray
  /// @param id id of the RegisterArray (not meaningful as of now)
  /// @param size number of registers in the RegisterArray
  /// @param bitwidth size of each register in bits
  void add_scheduler_parameter(const int &param_index, std::vector<int> value,
                               bm::p4object_id_t id,
                               size_t size = MAX_SIZE_SCHED_PARAM_ARRAY,
                               int bitwidth = BITWIDTH);

  int get_content_reg(int reg_index, int idx);

  /// @brief Return the predicate register. Thread safe access
  /// @return the rank of the predicate packet to be set as predicate in the
  /// node
  std::pair<int, int> get_predicate() const {
    // auto lock = predicate->unique_lock();
    int day = predicate->at(0).get<int>();
    int time = predicate->at(1).get<int>();
    // BMLOG_DEBUG("Read day:{}, time:{} from predicate register", day, time);
    return std::make_pair(day, time);
  }

  void find_next_non_empty_day(const Data &day, const Data &max_search_limit,
                               Data &next_day);

  // Inclusive non empty day
  void find_non_empty_day(const Data &day_start, const Data &max_search_limit,
                               Data &next_day);

  void has_packets(const Data &day, Data &is_empty);

  size_t get_field(int field_index);

 private:
  using Registers = std::unordered_map<int, std::unique_ptr<bm::RegisterArray>>;

  // Scheduler parameters set at P4Runtime
  // Should be read-only for the P4 user
  Registers scheduler_params;

  Registers packet_informations;

  // General purpose registers. To be used by the P4 user
  Registers gen_purpose_reg;

  std::unique_ptr<bm::RegisterArray> rank;
  std::unique_ptr<bm::RegisterArray> predicate;

  Node *owner;
};

}  // namespace bm

#endif  // SIMPLE_SWITCH_EXTERN_INTERFACE_TM_H_
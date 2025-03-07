#ifndef BM_BM_SIM_NODE_H
#define BM_BM_SIM_NODE_H

#include <bm/bm_sim/actions.h>  // bm::ActionFnEntry
#include <bm/bm_sim/calendar_item.h>
#include <bm/bm_sim/task.h>  // Task
#include <bm/config.h>

#include <condition_variable>
#include <fstream>  // std::ofstream
#include <iostream>
#include <map>      // std::map
#include <memory>   // std::shared_ptr
#include <queue>    // std::queue
#include <string>   // std::string
#include <utility>  // std::pair
#include <vector>   // std::vector

#include "../targets/simple_switch/extern/interface_tm.h"  // TrafficManagerInterface

namespace bm {

class TrafficManagerInterface;
class TrafficManager;
using PredicateCallback = std::function<void()>;

/**
 * @brief Node class. Base class of the traffic manager.
 * Act as a mini-version of the TrafficManager class. Can have children
 * and a parent (one only). If no parent is set, the node is a root node, linked
 * to an output port.
 */
class Node {
 public:
  Node();
  explicit Node(int id);
  Node(int id, TrafficManager *tm);
  Node(int id, TrafficManager *tm, std::string scheduler_type,
       int egress_port = -1);
  ~Node();

  /* Delete copy/move operators */
  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;
  Node(Node &&) = delete;
  Node &operator=(Node &&) = delete;

  void run(); /* Main Node's loop */
  void enqueue(Task &&task);
  void dequeue(std::pair<int, int> pred_to_dq);
  std::pair<int, int> calculate_rank(
      std::shared_ptr<bm::CalendarItem>);  // void ?
  void eval_predicate();
  void predicate_worker();
  void periodic_timeout();

  // Setup functions
  void set_actions(std::unordered_map<std::string, bm::ActionFn *> *actions);
  void set_parent(Node *parent);
  void set_children(const std::vector<std::shared_ptr<Node>> &children);

  // Access node related information
  std::shared_ptr<bm::CalendarItem> get_lowest_for_day(int day) const;
  std::shared_ptr<bm::CalendarItem> get_lowest() const;
  bool has_packets_for_day(int day) const;
  bool ready() { return !calendar_store.empty() && predicate_set; }
  int get_id() const { return id; }
  std::string get_scheduler_type() { return scheduler_type; }

 private:
  int id;
  bool root;
  std::string scheduler_type;
  int egress_port;

  std::unique_ptr<TrafficManagerInterface> node_p4_interface;
  std::map<std::pair<int, int>, std::shared_ptr<bm::CalendarItem>>
      calendar_store;
  TaskQueue task_queue;

  // Second item to 0 = nullptr predicate
  std::pair<int, int> predicate_rank;
  bool flag = false;
  std::atomic<bool> predicate_set{false};
  PredicateCallback predicate_callback;
  std::mutex eval_predicate_mutex;

  std::thread run_thread;
  std::thread predicate_thread;
  std::queue<std::function<void()>> predicate_tasks;
  std::mutex predicate_mutex;
  std::condition_variable predicate_cv;
  bool predicate_running = true;

  // P4 actions
  std::unordered_map<std::string, bm::ActionFnEntry *> actions_map;
  std::unordered_map<std::string, bm::ActionFn *> actionsfn_map;

  // Hierarchy management
  Node *parent;
  std::vector<std::shared_ptr<Node>> children;
  bm::TrafficManager *owner;

  // Thread safe access management
  std::condition_variable cv;  // Condition variable for task_queue
  std::mutex mtx;              // Mutex for task_queue

#ifdef BM_ENABLE_TM_DEBUG
  // Logs packet IDs and info to CSV
  std::ofstream csv_tm_dump_in;
  std::ofstream csv_tm_dump_out;  // Logs packet IDs and info to CSV

  std::string current_time();
  void dump_packet_info(std::ofstream &csv, bm::CalendarItem *cal_item);

  std::string calendar_store_to_string() const {
    std::stringstream ss;
    ss << "Calendar Store Contents:\n";
    for (const auto &entry : calendar_store) {
      ss << "Key: (" << entry.first.first << ", " << entry.first.second
         << ") -> ";
      if (entry.second != nullptr) {
        ss << "PacketID: "
           << entry.second->get_packet_id();  // assuming get_packet_id() exists
      } else {
        ss << "nullptr";
      }
      ss << "\n";
    }
    return ss.str();
  }

  // Vectors to store accumulated log entries
  std::vector<std::string> accumulated_logs_in;
  std::vector<std::string> accumulated_logs_out;

  void dump_packet_info_vector(std::vector<std::string> &vec,
                               bm::CalendarItem *cal_item);

 public:
  void write_accumulated_logs() {
    // Write accumulated incoming logs to file
    for (const auto &entry : accumulated_logs_in) {
      csv_tm_dump_in << entry << std::endl;
    }
    csv_tm_dump_in.flush();

    // Write accumulated outgoing logs to file
    for (const auto &entry : accumulated_logs_out) {
      csv_tm_dump_out << entry << std::endl;
    }
    csv_tm_dump_out.flush();

    // Clear the accumulated logs
    accumulated_logs_in.clear();
    accumulated_logs_out.clear();
  }
#endif
 private:
  int drank = 0;  // debug rank only
  int pkt_dequeued = 0;
};

}  // namespace bm

#endif  // BM_BM_SIM_NODE_H
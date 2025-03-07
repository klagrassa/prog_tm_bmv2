#include <bm/bm_sim/logger.h>
#include <bm/bm_sim/node.h>
#include <bm/bm_sim/traffic_manager.h>

bm::Node::Node() {
  BMLOG_DEBUG("Node created");

  this->id = 0;
  predicate_rank = std::make_pair(0, 0);
  scheduler_type = "FIFO";

  node_p4_interface = std::make_unique<TrafficManagerInterface>();
  node_p4_interface->init();
  node_p4_interface->set_owner(this);
}

bm::Node::Node(int id) {
  if (id >= 0) this->id = id;
  predicate_rank = std::make_pair(0, 0);
  // TODO change with runtime configuration
  // scheduler_type = "FIFO";
  // scheduler_type = "DRR";
  scheduler_type = "SP";

  node_p4_interface = std::make_unique<TrafficManagerInterface>();
  node_p4_interface->init();
  node_p4_interface->set_owner(this);

#ifdef BM_ENABLE_TM_DEBUG
  if (node_p4_interface) {
    BMLOG_DEBUG("Node {} created with P4 interface", this->id);
    std::cout << "Node " << this->id << " created with P4 interface"
              << std::endl;
  }
#endif

#ifdef BM_ENABLE_TM_DEBUG
  // Open CSV file and write header
  csv_tm_dump_in.open("packet_log_in" + std::to_string(id) + ".csv",
                      std::ios::out | std::ios::trunc);
  csv_tm_dump_out.open("packet_log_out" + std::to_string(id) + ".csv",
                       std::ios::out | std::ios::trunc);
  if (!csv_tm_dump_in.is_open() || !csv_tm_dump_out.is_open()) {
    BMLOG_DEBUG("Failed to open CSV file");
  }
  std::string header =
      "PacketID,EgressPort,PacketSize,Priority,DSCP,Color,VLANID,Sport,Dport\n";
  csv_tm_dump_in << header;
  csv_tm_dump_out << header;
  csv_tm_dump_in.flush();
  csv_tm_dump_out.flush();
#endif
}

bm::Node::Node(int id, TrafficManager* tm) : Node(id) {
  owner = tm;
}

bm::Node::~Node() {
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Node destroyed");
  if (csv_tm_dump_in.is_open()) csv_tm_dump_in.close();
  if (csv_tm_dump_out.is_open()) csv_tm_dump_out.close();
#endif
}

/**
 * @brief Set the actions map of the Node.
 *
 * @param actions Actions map to set.
 */
void bm::Node::set_actions(
    std::unordered_map<std::string, bm::ActionFn*>* actions) {
  this->actionsfn_map = *actions;
  auto* extern_ptr = node_p4_interface.get();

  // Inject extern to the P4 action
  for (auto& action : actionsfn_map) {
    bm::ActionFn* action_fn = action.second;
    // action_fn->parameter_push_back_extern_instance(extern_ptr);
    action_fn->update_extern_instance(extern_ptr);
  }

  // Populate actions_map with ActionFnEntry
  for (auto& action : actionsfn_map) {
    bm::ActionFn* action_fn = action.second;
    bm::ActionFnEntry* entry = new bm::ActionFnEntry(action_fn);
    actions_map[action.first] = entry;
  }
}

/**
 * @brief Set the parent of the Node.
 *
 * @param parent Parent to set.
 */
void bm::Node::set_parent(Node* parent) { this->parent = parent; }

/**
 * @brief Set the children of the Node.
 *
 * @param children Children to set.
 */
void bm::Node::set_children(
    const std::vector<std::shared_ptr<Node>>& children) {
  for (const auto& child : children) {
    this->children.push_back(
        child);  // copies the shared_ptr, original vector remains intact
  }
}

std::shared_ptr<bm::CalendarItem> bm::Node::get_lowest_for_day(int day) const {
  // Keys are ordered lexicographically (day, time), so lower_bound with (day,
  // 0)
  // returns the first CalendarItem for that day (if it exists).

  // FIRST TRY (using std libs)
  auto it = calendar_store.lower_bound(std::make_pair(day, 1));
  if (it == calendar_store.end() || it->first.first != day) {
    return nullptr;  // No entries for the given day.
  }

  return it->second;
}

/**
 * @brief Get the lowest-ranked CalendarItem in the Node.
 * Is get_lowest for the lowest day
 */
std::shared_ptr<bm::CalendarItem> bm::Node::get_lowest() const {
  if (calendar_store.empty()) {
    return nullptr;  // No items exist
  }
  return calendar_store.begin()
      ->second;  // The first element is the lowest-ranked
}

bool bm::Node::has_packets_for_day(int day) const {
  // calendar_store is keyed by a pair (day, time)
  auto it = calendar_store.lower_bound(std::make_pair(day, 0));
  // If found and the day component matches, there is at least one packet.
  return (it != calendar_store.end() && it->first.first == day);
}

void bm::Node::run() {
  while (true) {
    //! Task scheduler part
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] {
      return !task_queue.empty();
    });  // Wait until task_queue is not empty
    bm::Task task = std::move(task_queue.front());
    task_queue.erase(task_queue.begin());
    lock.unlock();

    switch (task.type) {
      case bm::TaskType::Enqueue: {
#ifdef BM_ENABLE_TM_DEBUG
        BMLOG_DEBUG("Task is enqueue");
#endif
        enqueue(std::move(task));
        break;

        case bm::TaskType::Dequeue:
#ifdef BM_ENABLE_TM_DEBUG
          BMLOG_DEBUG("Task is dequeue");
#endif
          break;
        default:
          std::cout << "Unknown task type in the Node" << std::endl;
      }
    }
  }  // end main loop
}

void bm::Node::enqueue(Task&& task) {
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Enqueued packet in the Node {}", this->id);
#endif
  std::shared_ptr<bm::CalendarItem> cal_item;
  cal_item = std::move(task.cal_item);

// Log packet details to CSV file
#ifdef BM_ENABLE_TM_DEBUG
  dump_packet_info(csv_tm_dump_in, cal_item.get());
#endif
  auto rank = this->calculate_rank(cal_item);
  cal_item->set_rank(rank);
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Rank is {}", rank.second);
#endif
  this->calendar_store.emplace(rank, std::move(cal_item));
#ifdef BM_ENABLE_TM_DEBUG
  // Display the content of the pkt store
  std::cout << calendar_store_to_string() << std::endl;
#endif

  // size_t wait_threasold = 10;
  // if (calendar_store.size() > wait_threasold) {
  //   flag = true;
  // }
  // if (flag) {
  //   std::cout << "FLAG IS TRUE : EVAL PREDICATE" << std::endl;
  //   eval_predicate();
  // }

  eval_predicate();
}

void bm::Node::dequeue3(std::pair<int, int> pred_to_dq) {
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Dequeuing packet from the Node - TASK VER");
#endif
  // Get the packet from the map
  auto it = calendar_store.find(pred_to_dq);
  if (it != calendar_store.end()) {
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("DQ - Packet found in the Node");
#endif
    std::shared_ptr<bm::CalendarItem> cal_item = std::move(it->second);

    Task task(TaskType::Dequeue, cal_item, this->id);
#ifdef BM_ENABLE_TM_DEBUG
    dump_packet_info(csv_tm_dump_out, cal_item.get());
    BMLOG_DEBUG("Task created for dequeue");
#endif
    owner->push_task(std::move(task));

    // P4 action called dequeued
    std::string action = scheduler_type + "_dequeued";
    actions_map[action]->execute(cal_item->get_packet_ptr());
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("Dequeued action called from the Node");
#endif
    if (calendar_store.empty()) {
#ifdef BM_ENABLE_TM_DEBUG
      BMLOG_DEBUG("Calendar store is empty after dequeue");
#endif
      predicate_rank = std::make_pair(0, 0);
      predicate_set = false;
    } else {
#ifdef BM_ENABLE_TM_DEBUG
      BMLOG_DEBUG("Evaluating predicate after dequeue");
#endif
      eval_predicate();
    }
  }
}

std::shared_ptr<bm::CalendarItem> bm::Node::dequeue() {
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Dequeued packet from the Node");
#endif

  // Additional safeguard (might be removed because of the safes from the TM)
  if (calendar_store.empty() or predicate_rank.second == 0) {
    return nullptr;
  }

  // Get the first item from the map
  auto it = calendar_store.begin();
  std::shared_ptr<bm::CalendarItem> cal_item = std::move(it->second);

  // P4 action called dequeued
  std::string action = scheduler_type + "_dequeued";
  actions_map[action]->execute(cal_item->get_packet_ptr());

#ifdef BM_ENABLE_TM_DEBUG
  dump_packet_info(csv_tm_dump_out, cal_item.get());
#endif

  // Remove the item from the map
  calendar_store.erase(it);

  // if (calendar_store.empty()) {
  //   predicate_rank = std::make_pair(0, 0);
  //   predicate_set = false;
  // }
  // else {
  //   eval_predicate();
  // }

  return cal_item;
}

void bm::Node::dequeue2(std::shared_ptr<bm::CalendarItem>& cal_item_out) {
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Dequeued packet from the Node");
#endif

  // Additional safeguard (might be removed because of the safes from the TM)
  if (calendar_store.empty() || predicate_rank.second == 0) {
    cal_item_out = nullptr;
    return;
  }

  // Get the first item from the map
  auto it = calendar_store.begin();
  std::shared_ptr<bm::CalendarItem> cal_item = std::move(it->second);

  // P4 action called dequeued
  std::string action = scheduler_type + "_dequeued";
  actions_map[action]->execute(cal_item->get_packet_ptr());

#ifdef BM_ENABLE_TM_DEBUG
  dump_packet_info(csv_tm_dump_out, cal_item.get());
#endif

  // Remove the item from the map
  calendar_store.erase(it);

  cal_item_out = std::move(cal_item);

  if (calendar_store.empty()) {
    predicate_rank = std::make_pair(0, 0);
    predicate_set = false;
  } else {
    eval_predicate();
  }
}

/**
 * @brief Calculate the rank of the CalendarItem.
 */
std::pair<int, int> bm::Node::calculate_rank(
    std::shared_ptr<bm::CalendarItem> cal_item) {
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Calculating rank in the Node");
#endif
  bm::Packet* pkt = cal_item->get_packet_ptr();
  std::string action = scheduler_type + "_calculate_rank";
  actions_map[action]->execute(pkt);
  auto rank = node_p4_interface->get_rank();
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Rank calculated");
#endif
  return std::make_pair(rank.first, rank.second);
}

/**
 * @brief Evaluate the predicate of the Node.
 */
void bm::Node::eval_predicate() {
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Evaluating predicate in the Node");
#endif

  // Safeguard, if the map is empty, return immediately
  auto it = calendar_store.begin();
  if (it == calendar_store.end()) {
    predicate_rank = std::make_pair(0, 0);
    predicate_set = false;
    return;
  }

  std::shared_ptr<bm::CalendarItem> cal_item = it->second;
  // Second safeguard, if the packet is empty, suppress return immediately
  // Really useful ? To check
  if (cal_item == nullptr) {
    predicate_rank = std::make_pair(0, 0);
    calendar_store.erase(it);
    predicate_set = false;
    return;
  }

  // P4 action call
  std::string action = scheduler_type + "_evaluate_predicate";
  actions_map[action]->execute(cal_item->get_packet_ptr());
  // New predicate is determined
  // Fetch the predicate
  auto new_pred = node_p4_interface->get_predicate();

  if (new_pred == std::make_pair(0, 0)) {
// If the predicate is empty, return immediately
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("Predicate is empty out of the P4 code");
#endif
    predicate_rank = std::make_pair(0, 0);
    predicate_set = false;
    return;
  }

  // Here, the current predicate cant be empty (returned earlier if its the
  // case) If new pred is different from the current one
  if (new_pred != predicate_rank) {
    /// If current pred is not empty and node is not root
    /// Dequeue current predicate packet
    if (predicate_rank.second != 0) {
      if (parent != nullptr) {
        // Dequeue the current predicate packet to parent

        // TEMPORARY IMPLEMENTATION, replace with dequeue to parent
        auto it = calendar_store.find(predicate_rank);
        if (it != calendar_store.end()) {
          calendar_store.erase(it);
        }
      }
    } else {
#ifdef BM_ENABLE_TM_DEBUG
      BMLOG_DEBUG("Predicate is non empty, but node is root");
#endif

      // Set the new predicate
      predicate_rank = new_pred;
      predicate_set = true;
    }

    /// Enqueue packet to parent node or TM
    if (this->ready()) {
      dequeue3(predicate_rank);
    }
  }

#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Predicate rank is {}, {}", predicate_rank.first,
              predicate_rank.second);
#endif
}

#ifdef BM_ENABLE_TM_DEBUG
/**
 * @brief Log packet details to CSV file.
 *
 * @param csv CSV file to write to.
 * @param cal_item CalendarItem to log.
 */
void bm::Node::dump_packet_info(std::ofstream& csv,
                                bm::CalendarItem* cal_item) {
  // Log packet details to CSV file
  csv << cal_item->get_packet_id() << "," << cal_item->get_egress_port() << ","
      << cal_item->get_packet_size() << ","
      << static_cast<int>(cal_item->get_priority()) << ","
      << static_cast<int>(cal_item->get_dscp()) << ","
      << static_cast<int>(cal_item->get_color()) << ","
      << static_cast<int>(cal_item->get_vlan_id()) << ","
      << static_cast<int>(cal_item->get_sport()) << ","
      << static_cast<int>(cal_item->get_dport()) << "\n";
  csv.flush();
}
#endif
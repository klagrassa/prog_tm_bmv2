#include <bm/bm_sim/logger.h>
#include <bm/bm_sim/task.h>  // Task
#include <bm/bm_sim/traffic_manager.h>

#include <iostream>
#include <sstream>

#include "jsoncpp/json.h"

bm::TrafficManager::TrafficManager()
    : pkt_store(EGRESS_PORT_NUMBER, 1024,
                TrafficManagerEgressThreadMapper(EGRESS_PORT_NUMBER)) {
  // Node creation
  std::unique_ptr<bm::Node> node = std::make_unique<bm::Node>(0, this);
  nodes_hierarchy.push_back(std::move(node));

  BMLOG_DEBUG("TrafficManager (default) created");
}

bm::TrafficManager::TrafficManager(
    bm::QueueingLogicPriRL<std::unique_ptr<Packet>, EgressThreadMapper>
        *egress_buffers)
    : TrafficManager() {
  egress_buf = egress_buffers;
  task_queue.reserve(1024);
  dequeue_thread = std::thread(&TrafficManager::dequeue_, this);

  config_server = std::make_unique<bm::ConfigServer>(41200);
  config_server_thread =
      std::thread([this]() { config_server->bind_and_listen(); });
  reconfiguration_thread = std::thread([this]() { run(); });

  BMLOG_DEBUG("TrafficManager (advanced task version) created");
  std::cout << "TrafficManager (advanced task version) created" << std::endl;
}

bm::TrafficManager::~TrafficManager() {
  BMLOG_DEBUG("TrafficManager destroyed");
  // Signal the TM run loop to stop and join the thread.
  stop_dequeue_thread = true;

  if (dequeue_thread.joinable()) {
    dequeue_thread.join();
  }
}

void bm::TrafficManager::run() {
  // Wait for config
  while (true) {
    if (config_server->is_config_ready()) {
      // Call the static parser
      auto config = config_server->get_config();
#ifdef BM_ENABLE_TM_DEBUG
      BMLOG_DEBUG("Config is ready for THE TM !");
      std::cout << config << std::endl;
      // Create the hierarchy and swap here (avoid destroyed node during swaps)
      std::cout << "[Configuration Parser] Parsing configuration" << std::endl;
#endif

      // parse the JSON config
      Json::Reader reader;
      Json::Value root;
      if (!reader.parse(config, root)) {
        std::cout << "[Configuration Parser] Failed to parse configuration"
                  << std::endl;
        continue;
      }

      // config loaded successfully
      std::vector<std::unique_ptr<bm::Node>> hierarchy = {};
      // std::vector<std::shared_ptr<bm::Node>> hierarchy = {};

      const Json::Value &tmconfig = root["tmconfig"];
      const Json::Value &tmnodes = tmconfig["tmnodes"];

      for (const auto &tmnode : tmnodes) {
        // mandatory fields
        int id = tmnode["tmnode"].asInt();
        std::string scheduler_type = tmnode["scheduler"].asString();

        // optional fields (root nodes, etc...)
        int egress_port;
        if (tmnode.isMember("port")) {
          egress_port = tmnode["port"].asInt();
        } else {
          egress_port = -1;
        }

        // auto node =
        //     // std::make_shared<bm::Node>(id, nullptr, scheduler_type,
        //     // egress_port);
        //     std::make_unique<bm::Node>(id, nullptr, scheduler_type,
        //                                egress_port);
        // // hierarchy.push_back(node);
        // hierarchy.push_back(
        //     std::move(node));  // Moves ownership instead of copying
        reconf_hierarchy.emplace_back(
            std::make_unique<bm::Node>(id, this, scheduler_type, egress_port));
      }

      // Lock to pause enqueuing
      {
        std::lock_guard<std::mutex> lock(enqueue_mutex);
        ready_to_enqueue = false;
      }

      // Wait for the packet store to be empty (avoid potential deadlocks)
      {
        std::unique_lock<std::mutex> lock(pkt_store_mutex);
        pkt_store_empty_cv.wait(lock, [this] { return pkt_store.empty(); });
      }
      // Dump logs
      // this->nodes_hierarchy[0]->write_accumulated_logs();
#ifdef BM_ENABLE_TM_DEBUG
      auto now = std::chrono::high_resolution_clock::now();
      auto now_c = std::chrono::system_clock::to_time_t(
          std::chrono::system_clock::now());
      auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        now.time_since_epoch()) %
                    1000000000;
      std::cout << "Reconfiguration started at "
                << std::put_time(std::localtime(&now_c), "%F %T") << "."
                << std::setfill('0') << std::setw(9) << now_ns.count()
                << std::endl;

      // Update the nodes hierarchy
      // nodes_hierarchy.clear();
      // BMLOG_DEBUG("Hierarchy cleared");
      // for (auto &node : new_hier) {
      //   nodes_hierarchy.push_back(std::move(node));
      // }
      // nodes_hierarchy = std::move(hierarchy);
      // reconf_hierarchy = std::move(hierarchy);
      BMLOG_DEBUG("Hierarchy swapped");
#endif
      swapped = true;
      // this->set_actions();
      this->set_actions(swapped);

      // Resume enqueuing
      {
        std::lock_guard<std::mutex> lock(enqueue_mutex);
        ready_to_enqueue = true;
      }
      enqueue_cv.notify_all();
#ifdef BM_ENABLE_TM_DEBUG
      BMLOG_DEBUG("Traffic Manager reconfigured");
#endif

      // Clear the config for the next iteration
      config_server->clear_config();
    }
  }
}

void bm::TrafficManager::dequeue_() {
  while (!stop_dequeue_thread) {
    //! Task scheduler part
    std::unique_lock<std::mutex> lock(tq_mutex);
    cv.wait(lock, [this] {
      return !task_queue.empty();
    });  // Wait until task_queue is not empty
    bm::Task task = std::move(task_queue.front());
    task_queue.erase(task_queue.begin());
    lock.unlock();

    // Extract the cal_item from Task
    std::shared_ptr<bm::CalendarItem> cal_item;
    cal_item = std::move(task.cal_item);
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("Dequeued packet from the Node, packet ID {}",
                cal_item->get_packet_id());
#endif

    // Check if non-null cal_item
    if (cal_item) {
      std::unique_ptr<Packet> packet;
      size_t queue_id;
      auto worker_id = TrafficManagerEgressThreadMapper(EGRESS_PORT_NUMBER)(
          cal_item->get_egress_port());
      std::unique_lock<std::mutex> lock(tm_mutex);  // Prevent
      pkt_store.pop_back(worker_id, &queue_id, &packet);
#ifdef BM_ENABLE_TM_DEBUG
      BMLOG_DEBUG("[THREAD {}] Dequeued packet from the TM, PacketID : {}",
                  std::this_thread::get_id(), packet->get_packet_id());
#endif
      egress_buf->push_front(queue_id, std::move(packet));
      // pkt_in_store--;
      // if (pkt_in_store >= 30) reconf_hierarchy[0]->write_accumulated_logs();
    } else {
      BMLOG_DEBUG("Cal_item is null");
    }
    if (pkt_store.empty()) {
      pkt_store_empty_cv.notify_one();
#ifdef BM_ENABLE_TM_DEBUG
      BMLOG_DEBUG("Packet store is empty");
#endif
    }
  }
}

/**
 * @brief Add action to the TM's action map
 */
void bm::TrafficManager::add_action(const std::string &type,
                                    ActionFn *action_fn) {
  actionsfn_map[type] = action_fn;
}

/**
 * @brief Set the actions to the node based on the scheduler type
 */
void bm::TrafficManager::set_actions() {
  for (auto &node : nodes_hierarchy) {
    node->set_actions(&actionsfn_map);
  }
}

void bm::TrafficManager::set_actions(bool swapped) {
  if (swapped) {
    for (auto &node : reconf_hierarchy) {
      node->set_actions(&actionsfn_map);
    }
  } else {
    for (auto &node : nodes_hierarchy) {
      node->set_actions(&actionsfn_map);
    }
  }
}

void bm::TrafficManager::push_task(Task &&task) {
  // std::lock_guard<std::mutex> lock(tm_mutex);  // Lock the mutex
  task_queue.push_back(std::move(task));
  cv.notify_one();
}

/**
 * @brief Enqueue a packet to the Traffic Manager stage.
 *
 * @param egress_port of the packet to be enqueued
 * @param packet to be enqueued
 */
void bm::TrafficManager::enqueue(uint32_t egress_port,
                                 std::unique_ptr<Packet> &&packet) {
  std::unique_lock<std::mutex> lock(enqueue_mutex);
  enqueue_cv.wait(lock, [this] { return ready_to_enqueue; });
  /** Mandatory workaround : If using only a raw pointer for the calendar
   * item, the packet will be deleted when the calendar item is deleted.
   * ie, Packet destructor will be called. This is not the desired behavior as
   * it removes the PHV. To avoid this, we use a shared_ptr with a no-op
   * deleter, specific to the packet.
   * This workaround is necessary because the packet is owned by the calendar
   * item, and the calendar item is owned by nodes.
   *
   * Before :
   * Destruction of CalendarItem -> Packet destructor called -> PHV reset
   *
   * After :
   * Destruction of CalendarItem -> Packet destructor not called -> PHV kept
   */

  auto no_op_deleter = [](bm::Packet *) { /* do nothing */ };
  std::shared_ptr<bm::Packet> non_owning_packet(packet.get(), no_op_deleter);
  std::shared_ptr<bm::CalendarItem> cal_item =
      std::make_shared<bm::CalendarItem>(non_owning_packet);
  cal_item->set_egress_port(egress_port);
#ifdef BM_ENABLE_TM_DEBUG
  BMLOG_DEBUG("Enqueued packet in the TM");
  BMLOG_DEBUG("Packet ID: {}", cal_item->get_packet_id());
  BMLOG_DEBUG("Egress port: {}", cal_item->get_egress_port());
#endif

  pkt_store.push_front(cal_item->get_egress_port(), std::move(packet));
  pkt_in_store++;
  if (pkt_in_store > 0) has_packet = true;
  cv.notify_one();

  // Send to first node
  // First create the enqueue task
  if (swapped) {
    Task task(TaskType::Enqueue, cal_item, this->reconf_hierarchy[0]->get_id());
    // Push it to the first node
    reconf_hierarchy[0]->enqueue(std::move(task));
  } else {
    Task task(TaskType::Enqueue, cal_item, this->nodes_hierarchy[0]->get_id());
    // Push it to the first node
    nodes_hierarchy[0]->enqueue(std::move(task));
  }
  // Task task(TaskType::Enqueue, cal_item, this->nodes_hierarchy[0]->get_id());
  // Push it to the first node
  // nodes_hierarchy[0]->enqueue(std::move(task));
#ifdef BM_ENABLE_TM_DEBUG
  // Display the content of the pkt store
  std::cout << pkt_store.to_string() << std::endl;
#endif
}

void bm::TrafficManager::reconfigure(Hierarchy new_hier) {
  // Lock to pause enqueuing
  {
    std::lock_guard<std::mutex> lock(enqueue_mutex);
    ready_to_enqueue = false;
  }

  // Wait for the packet store to be empty (avoid potential deadlocks)
  {
    std::unique_lock<std::mutex> lock(pkt_store_mutex);
    pkt_store_empty_cv.wait(lock, [this] { return pkt_store.empty(); });
  }

  // Update the nodes hierarchy
  nodes_hierarchy.clear();
  // for (auto &node : new_hier) {
  //   nodes_hierarchy.push_back(std::move(node));
  // }
  nodes_hierarchy = std::move(new_hier);
  this->set_actions();

  // Resume enqueuing
  {
    std::lock_guard<std::mutex> lock(enqueue_mutex);
    ready_to_enqueue = true;
  }
  enqueue_cv.notify_all();

  BMLOG_DEBUG("Traffic Manager reconfigured");
}

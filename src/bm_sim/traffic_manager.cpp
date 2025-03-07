#include <bm/bm_sim/config_server.h>  // ConfigServer
#include <bm/bm_sim/logger.h>
#include <bm/bm_sim/task.h>  // Task
#include <bm/bm_sim/traffic_manager.h>

#include <iostream>
#include <sstream>

bm::TrafficManager::TrafficManager()
    : pkt_store(EGRESS_PORT_NUMBER, 1024,
                TrafficManagerEgressThreadMapper(EGRESS_PORT_NUMBER)) {
  // Node creation
  std::unique_ptr<bm::Node> node = std::make_unique<bm::Node>(0, this);
  nodes_hierarchy.push_back(std::move(node));

  node_run_thread = std::thread(&bm::Node::run, nodes_hierarchy[0].get());
  BMLOG_DEBUG("TrafficManager (default) created");
}

bm::TrafficManager::TrafficManager(
    bm::QueueingLogicPriRL<std::unique_ptr<Packet>, EgressThreadMapper>
        *egress_buffers)
    : TrafficManager() {
  egress_buf = egress_buffers;
  task_queue.reserve(1024);
  dequeue_thread = std::thread(&TrafficManager::dequeue_, this);

  // bm::ConfigServer config_server(45000);

  BMLOG_DEBUG("TrafficManager (advanced task version) created");
}

bm::TrafficManager::~TrafficManager() {
  BMLOG_DEBUG("TrafficManager destroyed");
  // Signal the TM run loop to stop and join the thread.
  stop_dequeue_thread = true;

  if (node_run_thread.joinable()) node_run_thread.join();
  if (dequeue_thread.joinable()) {
    dequeue_thread.join();
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
    } else {
      BMLOG_DEBUG("Cal_item is null");
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
  /** Mandatory workaround : If using only a raw pointer for the calendar
   * item, the packet will be deleted when the calendar item is deleted.
   * ie, Packet destructor will be called. This is not the desired behavior as
   * it removes the PHV. To avoid this, we use a shared_ptr with a no-op
   * deleter, specific to the packet.
   * This workaround is necessary because the packet is owned by the calendar
   * item, and the calendar item is owned by nodes.
   * Main issue is that packet is unique_ptr, while multiple packets can be  
   * handled by nodes.
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

  // Enqueue first to main store to keep the packet_id
  // packets_store.emplace(cal_item->get_packet_id(), std::move(packet));

  // std::lock_guard<std::mutex> lock(tm_mutex);  // Lock the mutex
  pkt_store.push_front(cal_item->get_egress_port(), std::move(packet));
  pkt_in_store++;
  if (pkt_in_store > 0) has_packet = true;
  cv.notify_one();

  // Send to first node
  // First create the enqueue task
  Task task(TaskType::Enqueue, cal_item, this->nodes_hierarchy[0]->get_id());
  // Push it to the first node
  nodes_hierarchy[0]->enqueue(std::move(task));
#ifdef BM_ENABLE_TM_DEBUG
  // Display the content of the pkt store
  std::cout << pkt_store.to_string() << std::endl;
#endif
}

/**
 * @brief Dequeue first packet from the Traffic Manager main packet store.
 *
 * @return std::unique_ptr<bm::Packet> the packet dequeued
 */
std::unique_ptr<bm::Packet> bm::TrafficManager::dequeue() {
  // std::lock_guard<std::mutex> lock(tm_mutex);  // Lock the mutex
  if (pkt_store.empty()) return nullptr;

  std::shared_ptr<bm::CalendarItem> pkt_dequeued = nullptr;

  // Prepare hierarchy implementation
  for (auto &node : nodes_hierarchy) {
    bool is_ready = node->ready();
    BMLOG_DEBUG("Node {} is ready ?  {}", node->get_id(), is_ready);
    if (is_ready) {
      BMLOG_DEBUG("Node {} is ready", node->get_id());
      // Dequeue from the first node
      pkt_dequeued = std::move(node->dequeue());
    }
  }

  if (!pkt_dequeued) {
#ifdef BM_ENABLE_TM_DEBUG
    BMLOG_DEBUG("No packet found in calendar queue, returning nullptr");
#endif
    return nullptr;
  }

  std::unique_ptr<Packet> packet;
  size_t queue_id;
  auto worker_id = TrafficManagerEgressThreadMapper(EGRESS_PORT_NUMBER)(
      pkt_dequeued->get_egress_port());
  pkt_store.pop_back(worker_id, &queue_id, &packet);

  BMLOG_DEBUG("Dequeued packet from the TM");
  std::cout << pkt_store.to_string() << std::endl;
  return packet;
}
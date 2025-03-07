#ifndef BM_BM_SIM_TRAFFIC_MANAGER_H_
#define BM_BM_SIM_TRAFFIC_MANAGER_H_

#include <bm/bm_sim/actions.h>
#include <bm/bm_sim/calendar_item.h>
#include <bm/bm_sim/node.h>
#include <bm/bm_sim/packet.h>
#include <bm/bm_sim/queueing.h>
#include <bm/bm_sim/thread_mapper.h>
#include <bm/bm_sim/task.h> 

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#define EGRESS_PORT_NUMBER 4

namespace bm {

// Analog to the EgressThreadMapper from simple_switch
struct TrafficManagerEgressThreadMapper {
  explicit TrafficManagerEgressThreadMapper(size_t nb_threads)
      : nb_threads(nb_threads) {}

  size_t operator()(size_t egress_port) const {
    return egress_port % nb_threads;
  }

  size_t nb_threads;
};

class TrafficManager {
 public:
  TrafficManager();
  TrafficManager(
      bm::QueueingLogicPriRL<std::unique_ptr<Packet>, EgressThreadMapper> *);
  ~TrafficManager();

  void dequeue_();

  void enqueue(uint32_t egress_port, std::unique_ptr<Packet> &&packet);
  std::unique_ptr<Packet> dequeue();
  void push_task(Task &&task);

  void add_action(const std::string &type, ActionFn *action_fn);
  void set_node_action(std::string &type, int node_id);
  void set_actions();

 private:
  bm::QueueingLogic<std::unique_ptr<bm::Packet>,
                    TrafficManagerEgressThreadMapper>
      pkt_store;
  bm::QueueingLogicPriRL<std::unique_ptr<Packet>, EgressThreadMapper>
      *egress_buf;

  std::vector<std::unique_ptr<bm::Node>> nodes_hierarchy;
  std::thread node_run_thread;

  std::mutex tm_mutex;
  std::mutex tq_mutex;
  std::condition_variable cv;
  std::thread dequeue_thread;
  std::atomic<bool> stop_dequeue_thread{false};

  std::unordered_map<std::string, ActionFnEntry *> actions_map;
  std::unordered_map<std::string, ActionFn *> actionsfn_map;

  int drank = 0;  // debug rank only

  int pkt_in_store = 0;
  bool has_packet = false;

  TaskQueue task_queue;
};

}  // namespace bm

#endif  // BM_BM_SIM_TRAFFIC_MANAGER_H_
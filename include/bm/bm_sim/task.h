#ifndef BM_BM_SIM_TASK_H_
#define BM_BM_SIM_TASK_H_

#include <bm/bm_sim/calendar_item.h>

#include <memory>
#include <vector>

namespace bm {

/**
 * @brief An enum representing the type of task to be executed by the task
 * scheduler.
 */
enum TaskType { Enqueue, Dequeue /*, Reenqueue, Drop, Transmit */ };

/**
 * @brief A struct representing a task to be executed by the task scheduler.
 */
struct Task {
  TaskType type; /**< The type of task to be executed. */
  std::shared_ptr<bm::CalendarItem> cal_item; /**< A shared pointer to the
                                      calendar item associated with the task. */
  int node_id;      /**< The ID of the leaf node associated with the task. */
  bool transmitted; /**< A boolean indicating whether the packet has been
                       transmitted. */

  // Default constructor, enqueue by default
  Task() : type(Enqueue), cal_item(nullptr), node_id(0) { transmitted = false; }
  Task(TaskType type, std::shared_ptr<bm::CalendarItem> cal_item, int node_id)
      : type(type), cal_item(cal_item), node_id(node_id) {}
  Task(TaskType type, std::shared_ptr<bm::CalendarItem> cal_item, int node_id,
       bool transmitted)
      : type(type),
        cal_item(cal_item),
        node_id(node_id),
        transmitted(transmitted) {}

  // Copy constructor
  Task(const Task &other)
      : type(other.type),
        cal_item(other.cal_item),  // Copy the shared_ptr
        node_id(other.node_id),
        transmitted(other.transmitted) {}

  // Copy assignment operator
  Task &operator=(const Task &other) {
    if (this != &other) {
      // Copy everything except the cal_item
      type = other.type;
      node_id = other.node_id;
      transmitted = other.transmitted;

      // Copy the shared_ptr
      cal_item = other.cal_item;
    }
    return *this;
  }

  // Move constructor
  Task(Task &&other) noexcept
      : type(other.type),
        cal_item(other.cal_item),  // Copy the shared_ptr
        node_id(other.node_id),
        transmitted(other.transmitted) {
    // Set the source's cal_item to nullptr to avoid double deletion
    other.cal_item = nullptr;
  }

  // Move assignment operator
  Task &operator=(Task &&other) noexcept {
    if (this != &other) {
      // Move everything except the cal_item
      type = std::move(other.type);
      node_id = std::move(other.node_id);
      transmitted = std::move(other.transmitted);

      // Copy the shared_ptr
      cal_item = other.cal_item;

      // Set the source's cal_item to nullptr to avoid double deletion
      other.cal_item = nullptr;
    }
    return *this;
  }
};

using TaskQueue = std::vector<Task>;

}  // namespace bm

#endif  // BM_BM_SIM_TASK_H_
#ifndef BM_BM_SIM_CONFIG_SERVER_H_
#define BM_BM_SIM_CONFIG_SERVER_H_

#include <bm/bm_sim/node.h>

#include <thread>
#include <vector>

namespace bm {

const int MAX_RECONFIGURATION_NUMBER = 3;

class ConfigServer final {
 public:
  ConfigServer();
  ConfigServer(int port);
  ~ConfigServer();

  void bind_and_listen();
  void accept_and_read();
  void start_accept_and_read_thread();
  void parse_config(const std::string &config);

  bool is_config_ready() const;
  std::string get_config() const {return config;};
  void clear_config() {config = "";};
  // std::vector<std::shared_ptr<Node>> get_hierarchy() const;
  std::vector<std::unique_ptr<Node>> get_hierarchy();

 private:
  int ntw_socket;
  int port;
  bool config_ready = false;
  std::thread accept_and_read_thread;

  std::string config = "";

  std::vector<std::unique_ptr<Node>> hierarchy;
  // std::vector<std::shared_ptr<Node>> hierarchy;
};

class ConfigParser final {
 public:
  ConfigParser();
  ~ConfigParser();

  static std::vector<std::unique_ptr<Node>> parse(const std::string &config);
  // static std::vector<std::shared_ptr<Node>> parse(const std::string &config);
};

}  // namespace bm

#endif  // BM_BM_SIM_CONFIG_SERVER_H_
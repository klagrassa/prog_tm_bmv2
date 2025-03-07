#ifndef BM_BM_SIM_CONFIG_SERVER_H_
#define BM_BM_SIM_CONFIG_SERVER_H_

#include <thread>

namespace bm {

const int MAX_RECONFIGURATION_NUMBER = 3;

class ConfigServer final {
 public:
  ConfigServer();
  ConfigServer(int port);
  ~ConfigServer();

  void bind_and_listen(int port);
  void accept_and_read();
  void start_accept_and_read_thread();
  void parse_config(const std::string &config);

  void set_config_ready(bool ready);
  bool is_config_ready() const;

 private:
  int ntw_socket;
  int port;
  bool config_ready = false;
  std::thread accept_and_read_thread;
};

}  // namespace bm

#endif  // BM_BM_SIM_CONFIG_SERVER_H_
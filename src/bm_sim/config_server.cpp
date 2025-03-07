#include <bm/bm_sim/config_server.h>
#include <bm/bm_sim/logger.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <vector>

bm::ConfigServer::ConfigServer() {}

bm::ConfigServer::ConfigServer(int port) : port(port) {
  ntw_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (ntw_socket == -1) {
    BMLOG_ERROR("Failed to create socket");
    exit(1);
  }
  bind_and_listen(this->port);
}

bm::ConfigServer::~ConfigServer() {
  close(ntw_socket);
  if (accept_and_read_thread.joinable()) {
    accept_and_read_thread.join();
  }
}

/**
 * @brief Binds the server socket to the specified port and starts listening for
 * incoming connections.
 *
 * @param port The port number to bind the server socket to.
 */
void bm::ConfigServer::bind_and_listen(int port) {
  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  server_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(ntw_socket, (struct sockaddr *)&server_address,
           sizeof(server_address)) < 0) {
    BMLOG_ERROR("Failed to bind socket");
    exit(1);
  }

  if (listen(ntw_socket, MAX_RECONFIGURATION_NUMBER) < 0) {
    BMLOG_ERROR("Failed to listen on socket");
    exit(1);
  }

  BMLOG_DEBUG("[Configuration Server] Listening on port {}", port);
  std::cout << "[Configuration Server] Listening on port " << port << std::endl;
  start_accept_and_read_thread();
}

/**
 * @brief Starts the thread to accept and read incoming connections.
 *
 * Attempts to accept a connection on the network socket. Logs an error and
 * terminates the program if the connection fails.
 * Blocks until a connection is established.
 */
void bm::ConfigServer::start_accept_and_read_thread() {
  accept_and_read_thread = std::thread(&ConfigServer::accept_and_read, this);
}

void bm::ConfigServer::accept_and_read() {
  sockaddr_in client_address{};
  socklen_t client_address_len = sizeof(client_address);
  int client_socket = accept(ntw_socket, (struct sockaddr *)&client_address,
                             &client_address_len);
  if (client_socket < 0) {
    BMLOG_ERROR("Failed to accept connection");
    exit(1);
  }

  BMLOG_DEBUG("[Configuration Server] Connection accepted");

  std::vector<char> buffer(32768);
  int bytes_read = read(client_socket, buffer.data(), buffer.size());
  if (bytes_read < 0) {
    BMLOG_ERROR("Failed to read from socket");
    exit(1);
  }

  BMLOG_DEBUG("[Configuration Server] Received {} bytes", bytes_read);
  BMLOG_DEBUG("[Configuration Server] Message: {}", buffer.data());
  std::cout << "[Configuration Server] Received " << bytes_read << " bytes"
            << std::endl;
  std::cout << "[Configuration Server] Message: " << buffer.data() << std::endl;
  parse_config(buffer.data());
}

void bm::ConfigServer::parse_config(const std::string &config) {
  std::cout << "[Configuration Server] Parsing configuration" << std::endl;
  std::cout << "[Configuration Server] Configuration: " << config << std::endl;
  BMLOG_DEBUG("[Configuration Server] Parsing configuration");
  BMLOG_DEBUG("[Configuration Server] Configuration: {}", config);
}
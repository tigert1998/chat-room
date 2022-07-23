#include <glog/logging.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <argparse/argparse.hpp>
#include <iostream>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>

#include "message.h"

class ClientsPool {
 private:
  std::mutex mtx_;
  std::set<int> client_fds_;

 public:
  void Add(int fd) {
    std::unique_lock<std::mutex> lock(mtx_);
    LOG(INFO) << "fd " << fd << " is added";
    client_fds_.insert(fd);
  }
  void Remove(int fd) {
    std::unique_lock<std::mutex> lock(mtx_);
    LOG(INFO) << "fd " << fd << " is removed";
    client_fds_.erase(fd);
  }
  std::set<int> client_fds() {
    std::unique_lock<std::mutex> lock(mtx_);
    return client_fds_;
  }
};

class MessageQueue {
 private:
  std::shared_mutex mtx_;
  std::vector<std::string> queue_;
  std::condition_variable_any cv_;

 public:
  void Push(const std::string& message) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    queue_.push_back(message);
    const Message* ptr = reinterpret_cast<const Message*>(&message[0]);
    LOG(INFO) << ptr->name() << ": \"" << ptr->text() << "\"\n";
    cv_.notify_all();
  }
  void TryWait(int i) {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    if (i < queue_.size()) {
      // should not wait
      return;
    }

    cv_.wait(lock, [&]() { return i < queue_.size(); });
  }
  int Size() {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return queue_.size();
  }
  std::string message(int i) {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return queue_[i];
  }
};

ClientsPool clients_pool;
MessageQueue message_queue;

void SendLoop(int client_fd) {
  int i = 0;
  while (1) {
    int size = message_queue.Size();
    for (; i < size; i += 1) {
      auto buf = message_queue.message(i);
      int offset = 0;
      while (offset < buf.size()) {
        int size = send(client_fd, &buf[0] + offset, buf.size() - offset, 0);
        if (size <= 0) {
          return;
        }
        offset += size;
      }
    }
    message_queue.TryWait(i);
  }
}

void RecvLoop(int client_fd) {
  while (1) {
    char header_buf[sizeof(Message::Header)];
    int offset = 0;
    while (offset < sizeof(header_buf)) {
      int size =
          recv(client_fd, &header_buf + offset, sizeof(header_buf) - offset, 0);
      if (size == 0) {
        // client is closed
        clients_pool.Remove(client_fd);
        close(client_fd);
        return;
      }
      offset += size;
    }

    Message::Header* header = reinterpret_cast<Message::Header*>(header_buf);

    int message_len = sizeof(Message::Header) + header->payload_len();

    std::string message_buf(message_len, 0);
    std::copy(header_buf, header_buf + sizeof(header_buf), &message_buf[0]);
    offset = sizeof(Message::Header);

    while (offset < message_len) {
      int size =
          recv(client_fd, &message_buf[0] + offset, message_len - offset, 0);
      if (size == 0) {
        // client is closed
        clients_pool.Remove(client_fd);
        close(client_fd);
        return;
      }
      offset += size;
    }

    message_queue.Push(message_buf);
  }
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser program("Chat Room Server");
  program.add_argument("--port").scan<'d', int>();
  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  getaddrinfo(nullptr, std::to_string(program.get<int>("--port")).c_str(),
              &hints, &res);
  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  CHECK_GE(fd, 0);
  CHECK_EQ(bind(fd, res->ai_addr, res->ai_addrlen), 0);
  freeaddrinfo(res);

  CHECK_EQ(listen(fd, 20), 0);

  while (1) {
    sockaddr client_addr;
    socklen_t client_addr_size = sizeof(sockaddr);
    int client_fd = accept(fd, &client_addr, &client_addr_size);
    CHECK_GE(client_fd, 0);

    clients_pool.Add(client_fd);

    std::thread(RecvLoop, client_fd).detach();
    std::thread(SendLoop, client_fd).detach();
  }

  return 0;
}
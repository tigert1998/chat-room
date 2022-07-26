#include <glog/logging.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <argparse/argparse.hpp>
#include <iostream>
#include <thread>

#include "message.h"

void RecvLoop(int server_fd) {
  while (1) {
    char header_buf[sizeof(Message::Header)];
    int offset = 0;
    while (offset < sizeof(header_buf)) {
      int size =
          recv(server_fd, &header_buf + offset, sizeof(header_buf) - offset, 0);
      if (size == 0) {
        // server is closed
        close(server_fd);
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
          recv(server_fd, &message_buf[0] + offset, message_len - offset, 0);
      if (size == 0) {
        // server is closed
        close(server_fd);
        return;
      }
      offset += size;
    }

    Message* message = reinterpret_cast<Message*>(&message_buf[0]);
    std::cout << message->name() << ": \"" << message->text() << "\"\n"
              << std::flush;
  }
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser program("Chat Room Client");
  program.add_argument("--name");
  program.add_argument("--host");
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
  getaddrinfo(program.get<std::string>("--host").c_str(),
              std::to_string(program.get<int>("--port")).c_str(), &hints, &res);
  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  CHECK_GE(fd, 0);
  CHECK_EQ(connect(fd, res->ai_addr, res->ai_addrlen), 0);
  freeaddrinfo(res);

  std::thread(RecvLoop, fd).detach();

  std::string text;
  while (std::getline(std::cin, text)) {
    Message::Header header;
    header.name_len = program.get<std::string>("--name").size();
    header.text_len = text.size();

    int message_len = sizeof(header) + header.payload_len();
    std::string message_buf(message_len, 0);
    new (&message_buf[0]) Message(program.get<std::string>("--name"), text);

    int offset = 0;
    while (offset < message_len) {
      int size = send(fd, &message_buf[0] + offset, message_len - offset, 0);
      if (size <= 0) {
        close(fd);
        LOG(WARNING) << "server is closed";
        exit(1);
      }
      offset += size;
    }
  }

  close(fd);

  return 0;
}
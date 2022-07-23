#include <glog/logging.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <argparse/argparse.hpp>
#include <iostream>
#include <string>

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
  CHECK_GE(fd, 0) << "fail to initialize socket";
  CHECK_EQ(bind(fd, res->ai_addr, res->ai_addrlen), 0);
  freeaddrinfo(res);

  return 0;
}
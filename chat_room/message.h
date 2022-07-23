#ifndef CHAT_ROOM_MESSAGE_H_
#define CHAT_ROOM_MESSAGE_H_

#include <string>

struct Message {
  struct Header {
    int name_len, text_len;

    int payload_len() const { return name_len + text_len; }
  };

  Header header;
  char buf[0];

  Message(const std::string &name, const std::string &text) {
    header.name_len = name.size();
    header.text_len = text.size();
    std::copy(name.begin(), name.end(), buf);
    std::copy(text.begin(), text.end(), buf + header.name_len);
  }

  std::string name() const { return std::string(buf, buf + header.name_len); }
  std::string text() const {
    return std::string(buf + header.name_len,
                       buf + header.name_len + header.text_len);
  }
};

#endif
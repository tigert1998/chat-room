project(chat-room)

add_subdirectory("third_party/glog")
add_subdirectory("third_party/argparse")

add_executable(
    chat_room_server
    "chat_room/server.cc"
)
target_link_libraries(
    chat_room_server
    glog
    argparse
)
add_executable(
    chat_room_client
    "chat_room/client.cc"
)
target_link_libraries(
    chat_room_client
    glog
    argparse
)
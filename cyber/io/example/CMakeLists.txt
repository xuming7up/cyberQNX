project(io_example VERSION 1.0.0)

#set_module_output(../)

add_executable(tcp_echo_client
  tcp_echo_client.cc
)

if(CMAKE_SYSTEM_NAME MATCHES QNX)
  target_link_libraries(tcp_echo_client
        ${TARGET_NAME}
        socket
  )
else()
  target_link_libraries(tcp_echo_client
        ${TARGET_NAME}
  )
endif()

add_executable(tcp_echo_server
  tcp_echo_server.cc
)

if(CMAKE_SYSTEM_NAME MATCHES QNX)
  target_link_libraries(tcp_echo_server
      ${TARGET_NAME}
      socket
  )
else()
  target_link_libraries(tcp_echo_server
      ${TARGET_NAME}
  )
endif()

add_executable(udp_echo_client
  udp_echo_client.cc
)

if(CMAKE_SYSTEM_NAME MATCHES QNX)
  target_link_libraries(udp_echo_client
      ${TARGET_NAME}
      socket
  )
else()
  target_link_libraries(udp_echo_client
      ${TARGET_NAME}
  )
endif()

add_executable(udp_echo_server
  udp_echo_server.cc
)

if(CMAKE_SYSTEM_NAME MATCHES QNX)
  target_link_libraries(udp_echo_server
      ${TARGET_NAME}
      socket
  )
else()
  target_link_libraries(udp_echo_server
      ${TARGET_NAME}
  )
endif()

install(TARGETS tcp_echo_client tcp_echo_server udp_echo_client udp_echo_server
    RUNTIME DESTINATION bin
)

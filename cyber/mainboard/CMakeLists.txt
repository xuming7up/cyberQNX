project(mainboard VERSION 1.0.0)


add_executable(mainboard
  mainboard.cc
  module_argument.cc
  module_controller.cc
)

target_link_libraries(mainboard
    ${TARGET_NAME}
)


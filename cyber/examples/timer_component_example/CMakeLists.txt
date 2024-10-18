

# for a temporary workaround (adding the transport.cc file) to eliminate the component communication bug
add_library(timer_component_example SHARED
  timer_component_example.cc
  ${TEMPFILE}
)

target_link_libraries(timer_component_example
  ${TARGET_NAME}
  cyber_examples_proto
)

install(FILES timer.dag timer.launch
  DESTINATION share/examples/timer_component_example
)



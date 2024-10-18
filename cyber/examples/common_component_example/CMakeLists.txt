add_executable(channel_prediction_camera_writer
  channel_prediction_camera_writer.cc
)

target_link_libraries(channel_prediction_camera_writer
  ${TARGET_NAME}
  cyber_examples_proto
)

add_executable(channel_prediction_radar_writer
  channel_prediction_radar_writer.cc
)

target_link_libraries(channel_prediction_radar_writer
  ${TARGET_NAME}
  cyber_examples_proto
)

add_executable(channel_test_camera_writer
channel_test_camera_writer.cc
)

target_link_libraries(channel_test_camera_writer
  ${TARGET_NAME}
  cyber_examples_proto
)

add_executable(channel_test_radar_writer
channel_test_radar_writer.cc
)

target_link_libraries(channel_test_radar_writer
  ${TARGET_NAME}
  cyber_examples_proto
)

# for a temporary workaround (adding the transport.cc file) to eliminate the component communication bug
add_library(common_component_camera SHARED
  common_component_camera.cc
  ${TEMPFILE}
)

target_link_libraries(common_component_camera
  ${TARGET_NAME}
  cyber_examples_proto
)

# for a temporary workaround (adding the transport.cc file) to eliminate the component communication bug
add_library(common_component_radar SHARED
  common_component_radar.cc
  ${TEMPFILE}
)

target_link_libraries(common_component_radar  
  ${TARGET_NAME}
  cyber_examples_proto
)

install(FILES common_camera.dag common_camera.launch common_radar.dag common_radar.launch
  DESTINATION share/examples/common_component_example
)

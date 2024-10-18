project(tools VERSION 1.0.0)

if(${CMAKE_SYSTEM_NAME} STREQUAL "QNX")
set(ENV{product} "NOCARE")
endif() 
 
 
set(FFMPEG_LIBRARIES   
    "${THIRD_PARTY_LIB}/libavcodec.so"  
    "${THIRD_PARTY_LIB}/libavformat.so"  
    "${THIRD_PARTY_LIB}/libavutil.so"  
    "${THIRD_PARTY_LIB}/libswscale.so"
    "${THIRD_PARTY_LIB}/libswresample.so"
)  
 
include_directories(${THIRD_PARTY_INCLUDE})  

add_executable(cyber_monitor
  cyber_monitor/cyber_topology_message.cc
  cyber_monitor/general_channel_message.cc
  cyber_monitor/general_message.cc
  cyber_monitor/general_message_base.cc
  cyber_monitor/renderable_message.cc
  cyber_monitor/screen.cc
  cyber_monitor/main.cc
)
target_link_libraries(cyber_monitor
  ncurses
  ${TARGET_NAME}
)
# set_target_properties(cyber_monitor PROPERTIES
#   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_monitor/"
# )

# cyber_recorder
set(cyber_recorder_source_files
    cyber_recorder/main.cc
    cyber_recorder/info.cc
    cyber_recorder/recorder.cc
    cyber_recorder/recoverer.cc
    cyber_recorder/spliter.cc
    cyber_recorder/player/play_task.cc
    cyber_recorder/player/play_task_buffer.cc
    cyber_recorder/player/play_task_consumer.cc
    cyber_recorder/player/play_task_producer.cc
    cyber_recorder/player/player.cc
)
if (DEFINED ENV{product})
  if ($ENV{product} STREQUAL "p3663-0001" OR $ENV{product} STREQUAL "p3710-0010" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/drivers/dwcamera_sensing_world")
    find_package(CUDA)
    add_executable(cyber_recorder
      ${cyber_recorder_source_files}
      cyber_recorder/player/dwcamera_deserializer.cpp
      "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/drivers/dwcamera_sensing_world/neonutility.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/drivers/dwcamera_sensing_world/Log.cpp"
    )
    target_compile_definitions(cyber_recorder PUBLIC -D_ON_ORIN_)
    set(DRIVEWORKS_DIR "/usr/local/driveworks-5.4/")
    set(DRIVEWORKS_INCLUDE_DIR "${DRIVEWORKS_DIR}/include")
    set(DRIVEWORKS_LIBRARIES   
      "${DRIVEWORKS_DIR}lib/libdw_base.so"  
      "${DRIVEWORKS_DIR}lib/libdw_sensors.so"
    )  
    target_include_directories(cyber_recorder PUBLIC ${DRIVEWORKS_INCLUDE_DIR} ${CUDA_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/drivers/dwcamera_sensing_world" "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/drivers/dwcamera_sensing_world/include")
    target_link_libraries(cyber_recorder
      ${TARGET_NAME} 
      ${DRIVEWORKS_LIBRARIES}
      ${FFMPEG_LIBRARIES}
      camera_proto
      lidar_proto
    )
  else()
    add_executable(cyber_recorder
      ${cyber_recorder_source_files}
    )
    target_link_libraries(cyber_recorder
      ${TARGET_NAME}
      ${FFMPEG_LIBRARIES}
      camera_proto
      lidar_proto
    )
  endif()
endif()

# set_target_properties(cyber_recorder PROPERTIES
#   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_recorder/"
# )

# cyber_channel cyber_launch cyber_node cyber_service
# file(COPY
#   cyber_channel/cyber_channel.py
#   DESTINATION ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_channel/
# )
file(COPY
  cyber_launch/cyber_launch.py
  DESTINATION ${CMAKE_BINARY_DIR}/bin/
)
file(RENAME ${CMAKE_BINARY_DIR}/bin/cyber_launch.py  ${CMAKE_BINARY_DIR}/bin/cyber_launch)
# file(COPY
#   cyber_node/cyber_node.py
#   DESTINATION ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_node/
# )
# file(COPY
#   cyber_service/cyber_service.py
#   DESTINATION ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_service/
# )
# file(COPY
#   cyber_tools_auto_complete.bash
#   DESTINATION ${CMAKE_BINARY_DIR}
# )

# file(RENAME ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_channel/cyber_channel.py ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_channel/cyber_channel)
# file(RENAME ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_launch/cyber_launch.py ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_launch/cyber_launch)
# file(RENAME ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_node/cyber_node.py ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_node/cyber_node)
# file(RENAME ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_service/cyber_service.py ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_service/cyber_service)

# install(FILES
#   ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_channel/cyber_channel
#   ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_launch/cyber_launch
#   ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_node/cyber_node
#   ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_service/cyber_service
#   ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_monitor/cyber_monitor
#   ${CMAKE_BINARY_DIR}/bin/cyber/tools/cyber_recorder/cyber_recorder
#   PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
#   DESTINATION bin
# )
# install(FILES
#   ${CMAKE_BINARY_DIR}/cyber_tools_auto_complete.bash
#   PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
#   DESTINATION .
# )

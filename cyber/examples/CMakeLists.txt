project(examples VERSION 1.0.0)


add_executable(cyber_example_talker talker.cc)
target_link_libraries(cyber_example_talker
    ${TARGET_NAME}
    cyber_examples_proto
)

add_executable(cyber_example_listener listener.cc)
target_link_libraries(cyber_example_listener
    ${TARGET_NAME}
    cyber_examples_proto
)

add_executable(cyber_example_paramserver paramserver.cc)
target_link_libraries(cyber_example_paramserver
    ${TARGET_NAME}
)

add_executable(cyber_example_record record.cc)
target_link_libraries(cyber_example_record
    ${TARGET_NAME}
)

add_executable(cyber_example_service service.cc)
target_link_libraries(cyber_example_service
    ${TARGET_NAME}
    cyber_examples_proto
)

# for a temporary workaround (adding the transport.cc file) to eliminate the component communication bug
# add a transport.cc file to elimit the bug of referencing different instance (of Singleton)
if(${CMAKE_SYSTEM_NAME} STREQUAL "QNX")
set(TEMPFILE ${CMAKE_SOURCE_DIR}/cyber/transport/transport.cc)
else()
set(TEMPFILE "")
endif()

add_subdirectory(timer_component_example)
add_subdirectory(common_component_example)

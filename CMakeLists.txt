cmake_minimum_required(VERSION 3.16.8)
project(cyber VERSION 6.0.0)

include("cmake/config.cmake" OPTIONAL) #OPTIONAL

get_filename_component(ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)
set(CMAKE_CXX_STANDARD 14)
set(TARGET_NAME ${PROJECT_NAME})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(INSTALL_DIR "install.${CMAKE_SYSTEM_NAME}.${CMAKE_SYSTEM_PROCESSOR}")

message(STATUS "${YELLOW}SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}, CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}${RESET}")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release) # Debug/Release
endif()

enable_language(ASM)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
# ccache
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
  message(STATUS "### use ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif (CCACHE_FOUND)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

set(CROUTINE_FILE "cyber/croutine/detail/swap_x86_64.S")
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(CROUTINE_FILE "cyber/croutine/detail/swap_x86_64.S")
  message("### SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR} ${CROUTINE_FILE}")
else()
  set(CROUTINE_FILE "cyber/croutine/detail/swap_aarch64.S")
  message("### SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR} ${CROUTINE_FILE}")
endif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")

find_program(PROTOBUF_PROTOC_EXECUTABLE protoc ${CMAKE_CURRENT_SOURCE_DIR}/${INSTALL_DIR}/bin)
file(GLOB items "${PROJECT_SOURCE_DIR}/${INSTALL_DIR}/lib/cmake/*") 
foreach(third_lib ${items})
    if(IS_DIRECTORY ${third_lib})
      list(APPEND CMAKE_PREFIX_PATH ${third_lib})
    endif()
endforeach()
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${INSTALL_DIR}/lib/fastcdr/cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${INSTALL_DIR}/lib/fastrtps/cmake)
message("### CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

find_package(fastrtps REQUIRED)
find_package(fastcdr REQUIRED)
find_package(Protobuf REQUIRED)

set(CYBER_MSGS_CACHE ON CACHE BOOL "generate cyber proto msgs flag.")

message(STATUS"########################  INCLUDE and LIB DIR info  ########################")
message("       fastcdr_INCLUDE_DIR:   ${fastcdr_INCLUDE_DIR}")
message("       fastrtps_INCLUDE_DIR:  ${fastrtps_INCLUDE_DIR}")
message("       Protobuf_INCLUDE_DIRS: ${Protobuf_INCLUDE_DIRS}")
message("       Python_INCLUDE_DIRS:   ${Python_INCLUDE_DIRS}")
message("       Uuid_INCLUDE_DIRS:     ${Uuid_INCLUDE_DIRS}")
message("       glog_INCLUDE_DIRS:     ${glog_INCLUDE_DIRS}")
message("       Gflags_INCLUDE_DIRS:   ${Gflags_INCLUDE_DIRS}")

message("       fastcdr_LIB_DIR:         ${fastcdr_LIB_DIR}")
message("       fastrtps_LIB_DIR:        ${fastrtps_LIB_DIR}")
message("       Protobuf_LIBRARIES_DIRS: ${Protobuf_LIBRARIES_DIRS}")
message("       Python_LIBRARIES_DIRS:   ${Python_LIBRARIES_DIRS}")
message("       Uuid_LIBRARIES_DIRS:     ${Uuid_LIBRARIES_DIRS}")
message("       glog_LIBRARY_DIRS:       ${glog_LIBRARY_DIRS}")
message("       Gflags_LIBRARY_DIRS:     ${Gflags_LIBRARY_DIRS}")
message(STATUS"########################                           ########################")

set(THIRD_PARTY_INCLUDE "${ROOT_DIR}/${INSTALL_DIR}/include")
set(DEPENCENCY_INCLUDE_DIRS
  ${THIRD_PARTY_INCLUDE}
  #add other include path here
)

set(THIRD_PARTY_LIB "${ROOT_DIR}/${INSTALL_DIR}/lib")
set(DEPENCENCY_LIB_DIRS
  ${THIRD_PARTY_LIB}
  #add other lib path here
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DEPENCENCY_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/proto
)

link_directories(${DEPENCENCY_LIB_DIRS})

####################################################################################################################
#public macros
set(BUILD_BIN_OUT ${CMAKE_CURRENT_BINARY_DIR}/bin)

macro(set_default_output)  
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
endmacro()

#generate the proto files to *.pb.h and *.pb.cc
macro(gen_proto_src proto_files) 
    if(${CYBER_MSGS_CACHE})
      #set(CYBER_MSGS_CACHE OFF CACHE BOOL "generate cyber proto msgs flag." FORCE)
      FOREACH(FIL ${proto_files})
        message("### generate proto file: ${FIL}")
        GET_FILENAME_COMPONENT(FIL_WE ${FIL} NAME_WE)
        string(REGEX REPLACE ".+/(.+)\\..*" "\\1" FILE_NAME ${FIL})
        string(REGEX REPLACE "(.+)\\${FILE_NAME}.*" "\\1" FILE_PATH ${FIL})
        EXECUTE_PROCESS(
          COMMAND
          ${PROTOBUF_PROTOC_EXECUTABLE}
          -I${CMAKE_CURRENT_SOURCE_DIR}
          --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
          --python_out=${CMAKE_CURRENT_BINARY_DIR}
          ${FIL}
        )
      ENDFOREACH()
    endif(${CYBER_MSGS_CACHE})
endmacro()

macro(gen_proto_lib proto_dir out_obj)
    file(GLOB proto_files
      "${CMAKE_CURRENT_SOURCE_DIR}/${proto_dir}/*.proto"
    )
    if(NOT proto_files)  
        message("No proto files found in ${CMAKE_CURRENT_SOURCE_DIR}/${proto_dir}. ")  
        return()
    endif() 
    add_library(${out_obj} OBJECT "${proto_files}")
    
    target_link_libraries(${out_obj} PUBLIC protobuf)
    
    set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    
    protobuf_generate(
        TARGET ${out_obj}
        IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${proto_dir}"
        PROTOC_OUT_DIR "${PROTO_BINARY_DIR}")
endmacro()

####################################################################################################################
set_default_output()

gen_proto_lib("cyber/proto" cyber_proto)
gen_proto_lib("cyber/examples/proto" cyber_examples_proto)

file(GLOB CYBER_SRCS
    "cyber/base/*.cc"
    "cyber/blocker/*.cc"
    "cyber/class_loader/*.cc"
    "cyber/class_loader/utility/*.cc"
    "cyber/common/*.cc"
    "cyber/component/*.cc"
    "cyber/croutine/*.cc"
    "cyber/croutine/*/*.cc"
    "cyber/data/*.cc"
    "cyber/event/*.cc"
    "cyber/io/*.cc"
    "cyber/logger/*.cc"
    "cyber/message/*.cc"
    "cyber/node/*.cc"
    "cyber/parameter/*.cc"
    "cyber/plugin_manager/*.cc"
    "cyber/record/*.cc"
    "cyber/record/*/*.cc"
    "cyber/scheduler/*.cc"
    "cyber/scheduler/*/*.cc"
    "cyber/service/*.cc"
    "cyber/service_discovery/*.cc"
    "cyber/service_discovery/*/*.cc"
    "cyber/task/*.cc"
    "cyber/time/*.cc"
    "cyber/timer/*.cc"
    "cyber/transport/*.cc"
    "cyber/transport/*/*.cc"
    "cyber/*.cc"
    "cyber/sysmo/*.cc"
    ${CROUTINE_FILE}
)

list(FILTER CYBER_SRCS EXCLUDE REGEX .*test[.]cc)

add_library(${TARGET_NAME} SHARED
    ${CYBER_SRCS}
)

set(glog_LIBRARIES "glog")
set(Uuid_LIBRARIES "uuid")
set(Gflags_LIBRARIES "gflags")
set(Poco_LIBRARIES "PocoFoundation")
set(Protobuf_LIBRARIES "protobuf")

message(STATUS"########################  3rd PARTY LIBRARIES INFO  ########################")
message("       TARGET_NAME:         ${TARGET_NAME}")
message("       glog_LIBRARIES:      ${glog_LIBRARIES}")
message("       Gflags_LIBRARIES:    ${Gflags_LIBRARIES}")
message("       Protobuf_LIBRARIES:  ${Protobuf_LIBRARIES}")
message("       Uuid_LIBRARIES:      ${Uuid_LIBRARIES}")
message("       Poco_LIBRARIES:      ${Poco_LIBRARIES}")
message(STATUS"########################                            ########################")

target_link_libraries(${TARGET_NAME} 
    ${glog_LIBRARIES}
    ${Gflags_LIBRARIES}
    ${Uuid_LIBRARIES}
    ${Poco_LIBRARIES}
    fastrtps
    fastcdr
    atomic
    cyber_proto
)

# --------------------------------------------------------------
#                add subdirectory here
# --------------------------------------------------------------
add_subdirectory(cyber/mainboard)
add_subdirectory(cyber/examples)
add_subdirectory(cyber/io/example)
add_subdirectory(proto)
add_subdirectory(cyber/tools)
add_subdirectory(modules)

set(CYBER_MSGS_CACHE OFF CACHE BOOL "generate cyber proto msgs flag." FORCE)

install(DIRECTORY cyber/conf/
  DESTINATION share/conf/
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/
  DESTINATION bin/
  PATTERN "*"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ 
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/
  DESTINATION lib/
  PATTERN "*.so"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ 
)


# setup.zsh
configure_file("scripts/setup.sh.in" "${CMAKE_BINARY_DIR}/setup.rc" @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/setup.rc
  DESTINATION .
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ 
)

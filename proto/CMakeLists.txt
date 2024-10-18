set(common "common")

macro(gen_proto_so proto_dir out_so depend_so)
    file(GLOB proto_files
      "${CMAKE_CURRENT_SOURCE_DIR}/${proto_dir}/*.proto"
    )
    if(NOT proto_files)  
        message("No proto files found in ${CMAKE_CURRENT_SOURCE_DIR}/${proto_dir}. ")  
        return()
    endif() 
    
    add_library(${out_so}_target_objects OBJECT "${proto_files}")
    
    target_link_libraries(${out_so}_target_objects PUBLIC protobuf::libprotobuf) 
    
    set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    protobuf_generate(
        TARGET ${out_so}_target_objects
        IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${proto_dir}"
        PROTOC_OUT_DIR "${PROTO_BINARY_DIR}")

    add_library(${out_so} SHARED $<TARGET_OBJECTS:${out_so}_target_objects>)
    target_link_libraries(${out_so} PUBLIC ${depend_so}) 
endmacro()

gen_proto_so(${common} ${common}_proto "")

file(GLOB items RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/*") 
#message("items: ${items}")

foreach(sub_proto ${items})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${sub_proto})
        if(${sub_proto} STREQUAL ${common})
          #message("Ignored dir: ${sub_proto}")
          continue()
        endif()

        gen_proto_so(${sub_proto} ${sub_proto}_proto ${common}_proto)
    endif()
endforeach()
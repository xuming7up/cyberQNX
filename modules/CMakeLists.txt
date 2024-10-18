# --------------------------------------------------------------
#                public micros
# --------------------------------------------------------------
macro(install_conf_dag_files src_relative_dir target_share_relative_dir)
    
    install(DIRECTORY ${src_relative_dir}/conf/
      DESTINATION ${target_share_relative_dir}/conf/
    )
    
    install(DIRECTORY ${src_relative_dir}/dag/
      DESTINATION ${target_share_relative_dir}/dag/
    )
endmacro()  
# --------------------------------------------------------------
#                add subdirectory here
# --------------------------------------------------------------
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
add_subdirectory(tools/visualizer)
endif() 


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(Qt5 COMPONENTS Gui REQUIRED)
find_package(Qt5 COMPONENTS OpenGL REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

set(VISUALIZER_BIN cyber_visualizer)

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/uis)
file(GLOB VISUALIZER_SRC *.cc *.qrc)
file(GLOB VISUALIZER_QRC *.qrc)
file(GLOB VISUALIZER_UI uis/*.ui)

add_executable(${VISUALIZER_BIN} 
    # ${VISUALIZER_UI}
    ${VISUALIZER_SRC}
    # ${VISUALIZER_QRC}
    
)
target_include_directories(${VISUALIZER_BIN}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${VISUALIZER_BIN} 
    cyber 
    lidar_proto
    radar_proto
    camera_proto
    fastrtps 
    Qt5::Core
    Qt5::Gui 
    Qt5::OpenGL
    Qt5::Widgets
    pthread
    GL
)
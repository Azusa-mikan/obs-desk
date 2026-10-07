cmake_minimum_required(VERSION 3.16)

project(obs_control VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.3 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)

qt_standard_project_setup()

qt_add_executable(obs_control
    src/main.cpp

    src/appsettings.h
    src/appsettings.cpp

    src/protocol/obsprotocol.h
    src/protocol/obsclient.h
    src/protocol/obsclient.cpp

    src/model/obsstate.h
    src/model/obsstate.cpp

    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/connectwidget.h
    src/ui/connectwidget.cpp
    src/ui/dashboardwidget.h
    src/ui/dashboardwidget.cpp
    src/ui/audiorow.h
    src/ui/audiorow.cpp
)

target_include_directories(obs_control PRIVATE src)

target_link_libraries(obs_control PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

if(WIN32)
    set_target_properties(obs_control PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

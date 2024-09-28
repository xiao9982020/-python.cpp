cmake_minimum_required(VERSION 3.12)
project(BackUpYourUSBStorage)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(BackUpYourUSBStorage WIN32 main.cpp)
find_package(jsoncpp CONFIG REQUIRED)
target_link_libraries(BackUpYourUSBStorage PRIVATE JsonCpp::JsonCpp)

target_link_libraries(BackUpYourUSBStorage PRIVATE 
    shell32
    user32
    advapi32
)

set_target_properties(BackUpYourUSBStorage PROPERTIES
    WIN32_EXECUTABLE TRUE
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)

configure_file(${CMAKE_SOURCE_DIR}/config.json ${CMAKE_BINARY_DIR}/config.json COPYONLY)

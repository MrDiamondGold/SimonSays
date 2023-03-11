cmake_minimum_required(VERSION 3.11)

project(SimonSays VERSION 0.1)

# find_package(CSFML 2.5 COMPONENTS system window graphics audio network REQUIRED)

include_directories(include)

file(GLOB SOURCES src/*.c)

add_executable(SimonSays ${SOURCES})

target_link_libraries(SimonSays csfml-system csfml-window csfml-graphics csfml-audio sfml-network)

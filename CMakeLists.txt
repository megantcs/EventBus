cmake_minimum_required(VERSION 4.1)
project(EventBus)

set(CMAKE_CXX_STANDARD 20)

add_executable(EventBus
        example/main.cpp)

include_directories(include)
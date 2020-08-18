cmake_minimum_required(VERSION 3.4.1)

add_library(nbinjection SHARED main.cpp)

find_library(log-lib log)

target_link_libraries(nbinjection ${log-lib})
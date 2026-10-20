cmake_minimum_required(VERSION 3.20)

project(fplll_install_consumer LANGUAGES CXX)

find_package(fplll 5.5 CONFIG REQUIRED)

add_executable(fplll_consumer_shared consumer.cpp)
target_link_libraries(fplll_consumer_shared PRIVATE fplll::fplll)

add_executable(fplll_consumer_static consumer.cpp)
target_link_libraries(fplll_consumer_static PRIVATE fplll::fplll_static)

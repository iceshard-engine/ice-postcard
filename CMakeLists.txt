cmake_minimum_required(VERSION 3.15)
project(iceshard_postcard CXX)

add_library(postcard private/postcard.cxx)

target_include_directories(postcard PUBLIC public)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/public/ice"
    DESTINATION "include"
    FILES_MATCHING
    PATTERN "*.hxx"
)

install(TARGETS postcard)

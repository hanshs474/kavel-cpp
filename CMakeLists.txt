cmake_minimum_required(VERSION 3.15)
project(kavel VERSION 0.1.0 LANGUAGES CXX)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(kavel src/kavel.cpp)
target_compile_features(kavel PUBLIC cxx_std_17)
target_include_directories(kavel PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(kavel PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

option(KAVEL_BUILD_EXAMPLES "Build the example CLI" OFF)
if(KAVEL_BUILD_EXAMPLES)
  add_executable(kavel_generate examples/generate.cpp)
  target_link_libraries(kavel_generate PRIVATE kavel)
endif()

include(GNUInstallDirs)
install(TARGETS kavel ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

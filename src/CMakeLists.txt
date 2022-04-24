#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

cmake_minimum_required(VERSION 3.10)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BoostCMake)
include(FTXUICMake)

set(PROJECT_NAME "vm-manager")
project(${PROJECT_NAME})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-variable -Wno-narrowing")

find_package(PkgConfig REQUIRED)
include_directories(${DEPS_INCS_INCLUDE_DIRS})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${EP_BOOST_INC_DIR})

file(GLOB guest_src guest/*.cc)
file(GLOB services_src services/*.cc)
file(GLOB utils_src utils/*.cc)
add_executable(${PROJECT_NAME} vm_manager.cc ${guest_src} ${services_src} ${utils_src})

target_link_libraries(${PROJECT_NAME}
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
  PRIVATE ep_boost::program_options
  PRIVATE ep_boost::log
  PRIVATE ep_boost::log_setup
  PRIVATE ep_boost::thread
  PRIVATE ep_boost::filesystem
  PRIVATE ep_boost::chrono
  -lrt
)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

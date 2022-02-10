#
# Copyright (c) 2020 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

cmake_minimum_required(VERSION 3.10)

# --- Fetch FTXUI --------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v2.0.0
)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
  FetchContent_Populate(ftxui)
  add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
# -----------------------------------------------------------------------------

set(PROJECT_NAME "vm-manager")
project(${PROJECT_NAME})

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps_libs REQUIRED IMPORTED_TARGET glib-2.0 menu form ncurses uuid json-c)
pkg_search_module(DEPS_INCS REQUIRED glib-2.0 menu form ncurses uuid json-c)
include_directories(${DEPS_INCS_INCLUDE_DIRS})
#set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(include include/guest)

find_package(Boost REQUIRED COMPONENTS program_options)
#find_package(Boost)
include_directories(${Boost_INCLUDE_DIR})

file(GLOB guest_src guest/*.cc)

add_executable(${PROJECT_NAME} vm_manager.cc utils.cc ${guest_src})
target_link_libraries(${PROJECT_NAME}
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
  PkgConfig::deps_libs
  ${Boost_LIBRARIES}
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

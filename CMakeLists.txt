cmake_minimum_required(VERSION 2.8)

set(PROJECT_NAME_STR libatlasclient)
set(CMAKE_MACOSX_RPATH 1)
project(${PROJECT_NAME_STR})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)

option (ASAN
        "Build with address sanitizer enabled (ON) or disabled (OFF) - 2x slowdown when enabled"
        OFF)

# Code coverage for debug build
string(TOLOWER ${CMAKE_BUILD_TYPE} LC_CMAKE_BUILD_TYPE)
if(CMAKE_CXX_COMPILER_ID STREQUAL GNU AND LC_CMAKE_BUILD_TYPE STREQUAL debug)
  MESSAGE(STATUS "Setting up CodeCoverage")
  include(CodeCoverage)
  setup_target_for_coverage(${PROJECT_NAME}_coverage runtests coverage)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCURL_STATICLIB -DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -Werror -fno-rtti -Wall -Wno-missing-braces -fprofile-arcs -ftest-coverage")
else()
  if (ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCURL_STATICLIB -DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -Werror -fno-rtti -Wall -Wno-missing-braces -fno-omit-frame-pointer -fsanitize=address")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCURL_STATICLIB -DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -Werror -fno-rtti -Wall -Wno-missing-braces")
  endif()
endif()

include_directories(3rd-party ${CMAKE_CURRENT_BINARY_DIR})
add_executable(gen_perc_bucket_tags ./gen_perc_bucket_tags.cc)
add_executable(gen_perc_bucket_values ./gen_perc_bucket_values.cc)
add_executable(gen_valid_chars ./gen_valid_chars.cc)

# add the commands to generate the include files
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
  COMMAND gen_perc_bucket_tags ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
  DEPENDS gen_perc_bucket_tags
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc
  COMMAND gen_perc_bucket_values ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc
  DEPENDS gen_perc_bucket_values
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/valid_chars.inc
  COMMAND gen_valid_chars ${CMAKE_CURRENT_BINARY_DIR}/valid_chars.inc
  DEPENDS gen_valid_chars
)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    link_directories(/usr/lib/x86_64-linux-gnu)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB LIB_SOURCE_FILES
  atlas_client.cc atlas_client.h types.h
  meter/*.h meter/*.cc
  util/*.h util/*.cc util/*.c
  interpreter/*.h interpreter/*.cc)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Werror -Wall -std=c99 -fpic -D_GNU_SOURCE")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native")
set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

add_library(atlasclient SHARED ${LIB_SOURCE_FILES}
  ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
  ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc
  ${CMAKE_CURRENT_BINARY_DIR}/valid_chars.inc)

set_target_properties(atlasclient PROPERTIES COMPILE_FLAGS "-Wextra -Wno-missing-braces")


if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libcurl-linux.a libcurl.a COPYONLY)
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libpcre-linux.a libpcre.a COPYONLY)
  add_library(atlasclientStatic STATIC ${LIB_SOURCE_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
    ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc
    ${CMAKE_CURRENT_BINARY_DIR}/valid_chars.inc)
  
  set_target_properties(atlasclientStatic PROPERTIES OUTPUT_NAME atlasclient)
  target_link_libraries(atlasclientStatic ${CMAKE_BINARY_DIR}/libpcre.a ${CMAKE_BINARY_DIR}/libcurl.a z)
else()
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libcurl-osx.a libcurl.a COPYONLY)
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libpcre-osx.a libpcre.a COPYONLY)
endif()
target_link_libraries(atlasclient ${CMAKE_BINARY_DIR}/libpcre.a ${CMAKE_BINARY_DIR}/libcurl.a z )

set(APP_SOURCE_FILES main.cc)
add_executable(native_client ${APP_SOURCE_FILES})
set_target_properties(native_client PROPERTIES LINK_FLAGS "-Wl,-rpath,.")
target_link_libraries(native_client atlasclient pthread)

# test configuration
enable_testing()
file(GLOB TEST_SOURCE_FILES test/*.cc test/*.h)

add_executable(runtests ${TEST_SOURCE_FILES})
set_target_properties(runtests PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

add_library(gtest 3rd-party/src/gtest/gtest-all.cc)


add_custom_command(TARGET runtests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/test/resources $<TARGET_FILE_DIR:runtests>/resources)

target_link_libraries(runtests
        atlasclient
        pthread
        gtest)

add_test(runtests runtests)

install(TARGETS atlasclient DESTINATION lib)
install(FILES atlas_client.h types.h DESTINATION include/atlas)
install(DIRECTORY meter interpreter util DESTINATION include/atlas
        FILES_MATCHING PATTERN "*.h")
install(FILES resources/atlas-config-reference.json DESTINATION etc)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  install(TARGETS atlasclientStatic DESTINATION lib)
  install(FILES ${CMAKE_SOURCE_DIR}/bundle/libcurl-linux.a DESTINATION lib RENAME libatlascurl.a)
  install(FILES ${CMAKE_SOURCE_DIR}/bundle/libpcre-linux.a DESTINATION lib RENAME libatlaspcre.a)
endif()

add_subdirectory(bench)

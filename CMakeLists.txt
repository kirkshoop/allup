#            Copyright (c) Dean Michael Berris 2010.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

# Copyright (c) Dean Michael Berris 2010.
# Copyright (c) Google, Inc. 2012.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 2.8)
project(ALLUP)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
find_package( ICU )
find_package( OpenSSL )
find_package( Threads )

set(Boost_USE_MULTITHREADED ON)
set(Boost_COMPONENTS system regex date_time filesystem program_options )
find_package( Boost 1.53 REQUIRED ${Boost_COMPONENTS} )

set(CPP-NETLIB_SOURCE_DIR ~/source/cpp-netlib)
set(CPP-NETLIB_TARGET_DIR ~/source/cpp-netlib-build)
set(CPP-NETLIB_COMPONENTS uri message message-directives message-wrappers http-message http-message-wrappers constants http-client http-client-connections )
find_package( CPP-NETLIB REQUIRED ${CPP-NETLIB_COMPONENTS})

set(RXCPP_SOURCE_DIR ~/source/Rx/rx)
find_package( RXCPP REQUIRED )

set(CMAKE_VERBOSE_MAKEFILE true)

message("C++ Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message("C++ Flags: ${CMAKE_CXX_FLAGS} link flags: ${CMAKE_CXX_LINK_FLAGS}")
if (Boost_FOUND)
  if (MSVC)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
  endif(MSVC)
  if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0501)
  endif(WIN32)
  include_directories(${Boost_INCLUDE_DIRS})
endif(Boost_FOUND)

if (OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
endif (OPENSSL_FOUND)

if (${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
  INCLUDE(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG(-std=c++11 HAVE_STD11)
  if (HAVE_STD11)
    list(APPEND CMAKE_CXX_FLAGS "-std=c++11 -Wall")
  else()
    message(FATAL_ERROR "No advanced standard C++ support (-std=c++11 not defined).")
  endif()
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
  INCLUDE(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG(-std=c++11 HAVE_STD11)
  if (HAVE_STD11)
    if (CPP-NETLIB_DISABLE_LIBCXX)
      list(APPEND CMAKE_CXX_FLAGS "-std=c++11 -Wall")
      list(APPEND CMAKE_CXX_LINK_FLAGS "-std=c++11")
    else()
      list(APPEND CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ -Wall")
      list(APPEND CMAKE_CXX_LINK_FLAGS "-std=c++11 -stdlib=libc++")
    endif()
  else()
    message(FATAL_ERROR "No C++11 support for Clang version. Please upgrade Clang to a version supporting C++11.")
  endif()
  add_definitions(
  -DASIO_HAS_MOVE -DASIO_HAS_VARIADIC_TEMPLATES -DASIO_HAS_STD_SYSTEM_ERROR
  -DASIO_ERROR_CATEGORY_NOEXCEPT=noexcept -DASIO_HAS_STD_ARRAY
  -DASIO_HAS_STD_SHARED_PTR -DASIO_HAS_STD_ATOMIC -DASIO_HAS_STD_CHRONO
  -DASIO_HAS_STD_ADDRESSOFF -DASIO_HAS_STD_FUNCTION -DASIO_HAS_STD_TYPE_TRAITS)
endif()

include_directories(
  ${ALLUP_SOURCE_DIR} 
  ${CPP-NETLIB_INCLUDE_DIRS}
  ${RXCPP_INCLUDE_DIRS})

add_executable(allup atom.cpp rss.cpp main.cpp)

set(BOOST_CLIENT_LIBS
  ${Boost_DATE_TIME_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  )

target_link_libraries(allup
    ${BOOST_CLIENT_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CPP-NETLIB_REQUIRED_LIBRARY})

if (OPENSSL_FOUND)
  target_link_libraries(allup ${OPENSSL_LIBRARIES})
endif (OPENSSL_FOUND)

set_target_properties(allup PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ALLUP_BINARY_DIR})

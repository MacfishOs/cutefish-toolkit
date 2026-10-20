# Install script for directory: /home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit-base.so.0.1.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit-base.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/base/libcutefish-toolkit-base.so.0.1.0"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/base/libcutefish-toolkit-base.so.0"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit-base.so.0.1.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit-base.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/base/libcutefish-toolkit-base.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/base" TYPE FILE FILES
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/application_registry.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/auto_reset.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/byte_stream.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/collator.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/command_line.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/dbus.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/dbus_types.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/elapsed_timer.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/desktop_entry.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/event_loop.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/file.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/file_watcher.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/geometry.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/hash.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/json.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/local_socket.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/locale.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/logging.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/lru_cache.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/mime_apps.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/mime_database.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/paths.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/process.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/region.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/settings.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/signal.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/strings.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/thread_pool.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/timer.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/transform.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/url.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/uuid.h"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/base/weak_ptr.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/base/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()

# Install script for directory: /home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/base/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit.so.0.1.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/libcutefish-toolkit.so.0.1.0"
    "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/libcutefish-toolkit.so.0"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit.so.0.1.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcutefish-toolkit.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHANGE
           FILE "${file}"
           OLD_RPATH "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/base:"
           NEW_RPATH "")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/libcutefish-toolkit.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/about_dialog.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/animated_image.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/animation.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/application.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/bitmap.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/bitmap_painter.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/blurred_copy.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/fast_blur.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/bubble_shape.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/button.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/check_box.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/clipboard.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/color_id.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/color_mixer.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/color_provider.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/combo_box.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/dialog_window.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/drag.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/drop_down_popup.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/file_drop_area.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/embedded_gpu.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/embedded_host.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/embedded_window.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/event.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/frame_client.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/geometry.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/geometry_conversions.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/glyph_run.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/file_chooser.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/header_backdrop.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/header_button.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/icon.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/icon_image.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/image.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/image_view.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/keys.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/label.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/layout.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/list_view.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/menu.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/painter.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/path.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/progress_bar.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/cell_layout.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/round_image_button.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/scroll_bar.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/scroll_view.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/scroller.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/search_field.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/segmented_control.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/slider.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/styled_label.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/subsurface.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/switch.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/test_input.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/text_area.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/text_field.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/text_layout.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/theme.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/toast.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/trusted_connection.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/toggle_control.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/view.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/window.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/window_header.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cutefish-toolkit/ui" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/src/ui/window_tab_bar.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CutefishToolkit/CutefishToolkitTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CutefishToolkit/CutefishToolkitTargets.cmake"
         "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/CMakeFiles/Export/06613f36e577677ee2315baefc161fef/CutefishToolkitTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CutefishToolkit/CutefishToolkitTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CutefishToolkit/CutefishToolkitTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/CutefishToolkit" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/CMakeFiles/Export/06613f36e577677ee2315baefc161fef/CutefishToolkitTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/CutefishToolkit" TYPE FILE FILES "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/CMakeFiles/Export/06613f36e577677ee2315baefc161fef/CutefishToolkitTargets-release.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/jatin/Documents/development/cutefish-new/cutefish-toolkit/build/src/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()

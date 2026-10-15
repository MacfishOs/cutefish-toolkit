# Generated protocol marshalling; the sources live in the build tree.
set(CUTEFISH_PROTOCOL_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols)
file(MAKE_DIRECTORY ${CUTEFISH_PROTOCOL_DIR})

function(cutefish_wayland_protocol name xml)
    set(code ${CUTEFISH_PROTOCOL_DIR}/${name}-protocol.c)
    set(client ${CUTEFISH_PROTOCOL_DIR}/${name}-client-protocol.h)
    add_custom_command(OUTPUT ${code}
        COMMAND ${WAYLAND_SCANNER} private-code ${xml} ${code}
        DEPENDS ${xml} VERBATIM)
    add_custom_command(OUTPUT ${client}
        COMMAND ${WAYLAND_SCANNER} client-header ${xml} ${client}
        DEPENDS ${xml} VERBATIM)
    set(${name}_SOURCES ${code} ${client} PARENT_SCOPE)
endfunction()

cutefish_wayland_protocol(xdg-shell
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml)
cutefish_wayland_protocol(viewporter
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/viewporter/viewporter.xml)
cutefish_wayland_protocol(presentation-time
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/presentation-time/presentation-time.xml)
cutefish_wayland_protocol(fractional-scale-v1
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml)
cutefish_wayland_protocol(cursor-shape-v1
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml)
cutefish_wayland_protocol(xdg-dialog-v1
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml)
cutefish_wayland_protocol(xdg-activation-v1
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml)
cutefish_wayland_protocol(xdg-foreign-unstable-v2
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml)
cutefish_wayland_protocol(text-input-unstable-v3
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v3.xml)
cutefish_wayland_protocol(pointer-gestures-unstable-v1
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml)
cutefish_wayland_protocol(xdg-output-unstable-v1
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml)
# cursor-shape-v1 refers to the tablet tool interface.
cutefish_wayland_protocol(tablet-unstable-v2
    ${WAYLAND_PROTOCOLS_DATADIR}/unstable/tablet/tablet-unstable-v2.xml)
# Copies of cutefish-compositor's protocols; the copies must stay identical.
cutefish_wayland_protocol(cutefish-window-hints-v1
    ${PROJECT_SOURCE_DIR}/protocols/cutefish-window-hints-v1.xml)
cutefish_wayland_protocol(cutefish-surface-blur-v1
    ${PROJECT_SOURCE_DIR}/protocols/cutefish-surface-blur-v1.xml)

add_subdirectory(base)

set(CUTEFISH_TOOLKIT_PUBLIC_HEADERS
    ui/animation.h
    ui/application.h
    ui/button.h
    ui/check_box.h
    ui/clipboard.h
    ui/combo_box.h
    ui/dialog_window.h
    ui/event.h
    ui/frame_client.h
    ui/geometry.h
    ui/file_chooser.h
    ui/header_backdrop.h
    ui/header_button.h
    ui/icon.h
    ui/image.h
    ui/image_view.h
    ui/keys.h
    ui/label.h
    ui/layout.h
    ui/list_view.h
    ui/menu.h
    ui/painter.h
    ui/path.h
    ui/progress_bar.h
    ui/quick_layout.h
    ui/scroll_view.h
    ui/search_field.h
    ui/segmented_control.h
    ui/slider.h
    ui/switch.h
    ui/test_input.h
    ui/text_area.h
    ui/text_field.h
    ui/theme.h
    ui/toast.h
    ui/toggle_control.h
    ui/view.h
    ui/window.h
    ui/window_header.h
)

add_library(cutefish-toolkit SHARED
    gpu/egl_context.cpp
    wayland/connection.cpp
    wayland/data_device.cpp
    wayland/keyboard.cpp
    wayland/text_input.cpp
    wayland/shell_surface.cpp
    wayland/wayland_window.cpp
    ui/animation.cpp
    ui/appearance.cpp
    ui/application.cpp
    ui/button.cpp
    ui/check_box.cpp
    ui/clipboard.cpp
    ui/combo_box.cpp
    ui/dialog_window.cpp
    ui/editable_text.cpp
    ui/frame_client.cpp
    ui/geometry.cpp
    ui/file_chooser.cpp
    ui/header_backdrop.cpp
    ui/header_button.cpp
    ui/icon.cpp
    ui/icon_loader.cpp
    ui/image.cpp
    ui/image_decoder.cpp
    ui/image_view.cpp
    ui/label.cpp
    ui/line_breaks.cpp
    ui/layout.cpp
    ui/list_view.cpp
    ui/menu.cpp
    ui/painter.cpp
    ui/path.cpp
    ui/platform_window.cpp
    ui/progress_bar.cpp
    ui/quick_layer.cpp
    ui/quick_layout.cpp
    ui/quick_rectangle.cpp
    ui/rounded_path.cpp
    ui/rounded_shape.cpp
    ui/scroll_bar.cpp
    ui/scroll_view.cpp
    ui/scroller.cpp
    ui/search_field.cpp
    ui/segmented_control.cpp
    ui/slider.cpp
    ui/switch.cpp
    ui/test_input.cpp
    ui/text_area.cpp
    ui/text_field.cpp
    ui/text_context.cpp
    ui/text_lines.cpp
    ui/theme.cpp
    ui/thumbnail_cache.cpp
    ui/toast.cpp
    ui/toggle_control.cpp
    ui/view.cpp
    ui/window.cpp
    ui/window_compositing.cpp
    ui/window_restore.cpp
    ui/window_header.cpp
    ${xdg-shell_SOURCES}
    ${viewporter_SOURCES}
    ${presentation-time_SOURCES}
    ${fractional-scale-v1_SOURCES}
    ${cursor-shape-v1_SOURCES}
    ${pointer-gestures-unstable-v1_SOURCES}
    ${text-input-unstable-v3_SOURCES}
    ${xdg-dialog-v1_SOURCES}
    ${xdg-activation-v1_SOURCES}
    ${xdg-foreign-unstable-v2_SOURCES}
    ${xdg-output-unstable-v1_SOURCES}
    ${tablet-unstable-v2_SOURCES}
    ${cutefish-window-hints-v1_SOURCES}
    ${cutefish-surface-blur-v1_SOURCES}
)

# The glyph icons, built in so they need no install location.
file(GLOB CUTEFISH_TOOLKIT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/data/icons/*.svg)
set(CUTEFISH_TOOLKIT_EMBEDDED ${CMAKE_CURRENT_BINARY_DIR}/embedded_files.cpp)
# A list would be split into separate arguments on the way to the script.
string(REPLACE ";" "|" CUTEFISH_TOOLKIT_ICON_LIST "${CUTEFISH_TOOLKIT_ICONS}")
add_custom_command(OUTPUT ${CUTEFISH_TOOLKIT_EMBEDDED}
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${CUTEFISH_TOOLKIT_EMBEDDED}
            -DFILES=${CUTEFISH_TOOLKIT_ICON_LIST} -P ${PROJECT_SOURCE_DIR}/cmake/EmbedFiles.cmake
    DEPENDS ${CUTEFISH_TOOLKIT_ICONS} ${PROJECT_SOURCE_DIR}/cmake/EmbedFiles.cmake
    VERBATIM)
target_sources(cutefish-toolkit PRIVATE ${CUTEFISH_TOOLKIT_EMBEDDED})

add_library(Cutefish::Toolkit ALIAS cutefish-toolkit)

set_target_properties(cutefish-toolkit PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME Toolkit
)

target_include_directories(cutefish-toolkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cutefish-toolkit>
    PRIVATE ${CUTEFISH_PROTOCOL_DIR}
)

target_link_libraries(cutefish-toolkit
    PUBLIC
        cutefish-toolkit-base
    PRIVATE
        skia
        PkgConfig::WaylandClient
        PkgConfig::Egl
        PkgConfig::Fontconfig
        PkgConfig::Xkbcommon
        PkgConfig::Systemd
        PkgConfig::ImageCodecs
)

install(TARGETS cutefish-toolkit EXPORT CutefishToolkitTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

foreach(header ${CUTEFISH_TOOLKIT_PUBLIC_HEADERS})
    get_filename_component(directory ${header} DIRECTORY)
    install(FILES ${header} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cutefish-toolkit/${directory})
endforeach()

install(EXPORT CutefishToolkitTargets
    NAMESPACE Cutefish::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CutefishToolkit
)

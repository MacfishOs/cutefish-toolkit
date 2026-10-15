# The part of the toolkit that needs neither a display nor Skia, for
# services and daemons as well as applications.
set(CUTEFISH_TOOLKIT_BASE_PUBLIC_HEADERS
    application_registry.h
    dbus.h
    dbus_types.h
    desktop_entry.h
    event_loop.h
    file.h
    file_watcher.h
    json.h
    locale.h
    mime_apps.h
    mime_database.h
    paths.h
    process.h
    settings.h
    signal.h
    thread_pool.h
    timer.h
)

add_library(cutefish-toolkit-base SHARED
    application_registry.cpp
    dbus.cpp
    desktop_entry.cpp
    event_loop.cpp
    file.cpp
    file_watcher.cpp
    json.cpp
    locale.cpp
    mime_apps.cpp
    mime_database.cpp
    paths.cpp
    process.cpp
    settings.cpp
    thread_pool.cpp
    timer.cpp
)

add_library(Cutefish::ToolkitBase ALIAS cutefish-toolkit-base)

set_target_properties(cutefish-toolkit-base PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME ToolkitBase
)

target_include_directories(cutefish-toolkit-base
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cutefish-toolkit>
)

target_link_libraries(cutefish-toolkit-base
    PRIVATE
        PkgConfig::Icu
        PkgConfig::Systemd
        Threads::Threads
)

install(TARGETS cutefish-toolkit-base EXPORT CutefishToolkitTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${CUTEFISH_TOOLKIT_BASE_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cutefish-toolkit/base
)

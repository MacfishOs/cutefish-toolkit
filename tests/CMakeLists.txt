find_package(GTest REQUIRED)
include(GoogleTest)

# Linked against the base library alone, which proves it needs no display.
add_executable(cutefish-toolkit-base-tests
    base/dbus_test.cpp
    base/desktop_entry_test.cpp
    base/event_loop_test.cpp
    base/file_test.cpp
    base/hash_test.cpp
    base/helpers_test.cpp
    base/io_helpers_test.cpp
    base/geometry_test.cpp
    base/json_test.cpp
    base/locale_test.cpp
    base/logging_test.cpp
    base/mime_test.cpp
    base/process_test.cpp
    base/region_test.cpp
    base/settings_test.cpp
    base/signal_test.cpp
    base/strings_test.cpp
    base/thread_pool_test.cpp
    base/timer_test.cpp
    base/transform_test.cpp
    base/user_directory_test.cpp
)

target_link_libraries(cutefish-toolkit-base-tests PRIVATE cutefish-toolkit-base GTest::gtest_main)

gtest_discover_tests(cutefish-toolkit-base-tests)

add_executable(cutefish-toolkit-tests
    main.cpp
    animation_test.cpp
    bitmap_test.cpp
    color_test.cpp
    controls_test.cpp
    frame_cost_test.cpp
    image_view_test.cpp
    input_test.cpp
    label_test.cpp
    layout_test.cpp
    line_breaks_test.cpp
    menu_test.cpp
    painter_test.cpp
    pixel_test.cpp
    quick_layout_test.cpp
    scroll_test.cpp
    text_area_test.cpp
    text_field_test.cpp
    text_metrics_test.cpp
    window_test.cpp
    window_restore_test.cpp
    compositing_test.cpp
    test_views.h
)

# The image tests write their files with the codecs' own encoders.
pkg_check_modules(ImageTestEncoders REQUIRED IMPORTED_TARGET libjpeg libwebp libtiff-4 libjxl)
target_link_libraries(cutefish-toolkit-tests PRIVATE cutefish-toolkit GTest::gtest PkgConfig::ImageTestEncoders)

gtest_discover_tests(cutefish-toolkit-tests)

# One Application per process: the embedded platform gets its own binary.
add_executable(cutefish-toolkit-embedded-tests
    embedded/main.cpp
    embedded/embedded_window_test.cpp
    embedded/embedded_controls_test.cpp
    embedded/embedded_gpu_test.cpp
)

target_link_libraries(cutefish-toolkit-embedded-tests PRIVATE cutefish-toolkit GTest::gtest PkgConfig::Xkbcommon
    PkgConfig::Egl PkgConfig::GlesV2)

gtest_discover_tests(cutefish-toolkit-embedded-tests)

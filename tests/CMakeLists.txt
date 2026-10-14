find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(cutefish-toolkit-tests
    main.cpp
    animation_test.cpp
    color_test.cpp
    controls_test.cpp
    event_loop_test.cpp
    input_test.cpp
    layout_test.cpp
    menu_test.cpp
    pixel_test.cpp
    scroll_test.cpp
    text_field_test.cpp
    window_test.cpp
    compositing_test.cpp
    test_views.h
)

target_link_libraries(cutefish-toolkit-tests PRIVATE cutefish-toolkit GTest::gtest)

gtest_discover_tests(cutefish-toolkit-tests)

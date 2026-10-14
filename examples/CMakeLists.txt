add_executable(cutefish-toolkit-demo
    demo/main.cpp
)

target_link_libraries(cutefish-toolkit-demo PRIVATE cutefish-toolkit)

add_executable(cutefish-toolkit-gallery
    gallery/main.cpp
)

target_link_libraries(cutefish-toolkit-gallery PRIVATE cutefish-toolkit)

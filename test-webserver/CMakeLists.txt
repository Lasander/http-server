cmake_minimum_required(VERSION 3.9.2)
project(webserver
    LANGUAGES CXX
)

add_executable(test-webserver
    main.cpp
)

target_link_libraries(test-webserver
    http-server
)

install(TARGETS test-webserver RUNTIME DESTINATION bin)
install(DIRECTORY web_root DESTINATION .)

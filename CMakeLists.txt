cmake_minimum_required(VERSION 3.9.2)
project(http-server
    LANGUAGES CXX
)

# pt library
add_library(http-server
    src/http_server.cpp
)

target_include_directories(http-server
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(http-server
    PUBLIC
        cxx_std_14
)

find_package(civetweb REQUIRED)
target_link_libraries(http-server
    civetweb::civetweb
)

# library installation
include(GNUInstallDirs)

install(
    TARGETS
        http-server
    EXPORT
        http-server-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    EXPORT http-server-targets
    FILE http-server-targets.cmake
    NAMESPACE http-server::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/http-server
)

# test-server
add_subdirectory(test-webserver)

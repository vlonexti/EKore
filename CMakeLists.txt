cmake_minimum_required(VERSION 3.21)
project(EKore VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "EKore supports Windows only.")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

add_library(EKore STATIC
    src/Process.cpp
    src/Pattern.cpp
    src/Patch.cpp)
add_library(EKore::Library ALIAS EKore)

target_compile_features(EKore PUBLIC cxx_std_20)
target_include_directories(EKore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(EKore PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE)

if(MSVC)
    target_compile_options(EKore PRIVATE /W4 /permissive- /utf-8 /MP)
endif()

if(BUILD_TESTING)
    add_executable(EKoreTests tests/EKoreTests.cpp)
    target_link_libraries(EKoreTests PRIVATE EKore::Library)
    if(MSVC)
        target_compile_options(EKoreTests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME EKoreTests COMMAND EKoreTests)
endif()

install(TARGETS EKore
    EXPORT EKoreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/EKore DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT EKoreTargets
    FILE EKoreTargets.cmake
    NAMESPACE EKore::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/EKore)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/EKoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EKoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/EKoreConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/EKore)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/EKoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/EKoreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/EKore)


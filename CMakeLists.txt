cmake_minimum_required(VERSION 3.21)
project(EKore VERSION 1.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "EKore supports Windows only.")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(EKORE_MENU "Build the ImGui MenuKit API" ON)

if(EKORE_MENU)
    set(EKORE_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
    add_library(ekore_imgui STATIC
        ${EKORE_IMGUI_DIR}/imgui.cpp
        ${EKORE_IMGUI_DIR}/imgui_draw.cpp
        ${EKORE_IMGUI_DIR}/imgui_tables.cpp
        ${EKORE_IMGUI_DIR}/imgui_widgets.cpp
        ${EKORE_IMGUI_DIR}/imgui_demo.cpp
        ${EKORE_IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
        ${EKORE_IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${EKORE_IMGUI_DIR}/backends/imgui_impl_dx11.cpp)
    target_include_directories(ekore_imgui PUBLIC
        $<BUILD_INTERFACE:${EKORE_IMGUI_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_definitions(
        ekore_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
endif()

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

if(EKORE_MENU)
    target_sources(EKore PRIVATE src/UI/MenuHost.cpp)
    target_link_libraries(EKore PUBLIC ekore_imgui)
    target_link_libraries(EKore PUBLIC d3d11 dxgi)
    target_compile_definitions(EKore PUBLIC EKORE_MENU=1)
endif()

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

set(EKORE_EXPORT_TARGETS EKore)
if(EKORE_MENU)
    list(APPEND EKORE_EXPORT_TARGETS ekore_imgui)
endif()

install(TARGETS ${EKORE_EXPORT_TARGETS}
    EXPORT EKoreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/EKore DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/EKore/docs)
install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/EKore)
if(EKORE_MENU)
    install(
        DIRECTORY third_party/imgui/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")
    install(
        FILES third_party/imgui/LICENSE.txt
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/EKore/licenses)
endif()

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

cmake_minimum_required(VERSION 3.16)

project(xreply VERSION 0.2 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    message(WARNING
        "CEF libraries are compiled with MSVC. Using ${CMAKE_CXX_COMPILER_ID} may cause link errors.")
endif()

if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/Qt/6.10.3/msvc2022_64")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network)

set(CEF_ROOT "D:/soft/cef_binary_154.0.34+g14c5a08+chromium-154.0.8037.98_windows64")
set(CEF_INCLUDE_PATH "${CEF_ROOT}" "${CEF_ROOT}/include")

set(XREPLY_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/AutoReplier.h
    src/AutoReplier.cpp
    src/ReplyStore.h
    src/ReplyStore.cpp
    src/FollowingStore.h
    src/FollowingStore.cpp
    src/App/CefApp.h
    src/App/CefApp.cpp
    src/App/CefHandler.h
    src/App/CefHandler.cpp
    src/UI/BrowserWidget.h
    src/UI/BrowserWidget.cpp
    src/Utils/Logger.h
    src/Utils/Logger.cpp
)

qt_add_executable(xreply
    MANUAL_FINALIZATION
    ${XREPLY_SOURCES}
)

target_include_directories(xreply PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CEF_INCLUDE_PATH}
)

target_compile_definitions(xreply PRIVATE
    WIN32
    _WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(xreply PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    "${CEF_ROOT}/Release/libcef.lib"
    "${CEF_ROOT}/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib"
    comctl32
    rpcrt4
    shlwapi
    ws2_32
    delayimp
)

if(MSVC)
    target_link_options(xreply PRIVATE /DELAYLOAD:libcef.dll)
    target_compile_options(xreply PRIVATE /utf-8)
endif()

set_target_properties(xreply PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/xreply"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/xreply"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/xreply"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/xreply"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/xreply"
)

add_custom_command(TARGET xreply POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_ROOT}/Release"
        "$<TARGET_FILE_DIR:xreply>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_ROOT}/Resources"
        "$<TARGET_FILE_DIR:xreply>"
)

if(WIN32)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET xreply POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                $<TARGET_FILE:xreply>
            COMMENT "Running windeployqt for xreply..."
        )
    endif()
endif()

qt_finalize_executable(xreply)

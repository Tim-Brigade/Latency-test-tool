cmake_minimum_required(VERSION 3.20)
project(LatencyTestTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

# FFmpeg - use vcpkg's find_package
find_package(FFMPEG REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/App.cpp
    src/TimestampDisplay.cpp
    src/VideoDecoder.cpp
    src/VideoRenderer.cpp
    src/Config.cpp
)

set(HEADERS
    src/App.h
    src/TimestampDisplay.h
    src/VideoDecoder.h
    src/VideoRenderer.h
    src/Config.h
)

# Create executable (WIN32 hides console window on Windows)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
    ${FFMPEG_LIBRARIES}
)

# Copy resources
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    # Link Windows libraries needed by FFmpeg
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32
        secur32
        bcrypt
        strmiids
        mfuuid
    )
endif()

# Set working directory for debugging
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

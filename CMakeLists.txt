cmake_minimum_required(VERSION 3.10)
project(mqtt_console)

set(CMAKE_CXX_STANDARD 17)

# Dependency checks and auto-install for Debian/Ubuntu
# Paho MQTT C++ must be built from source with the C library included.
# See README for instructions.
 ##--- Paho MQTT C++ ---
include(FetchContent)

# Try to find Paho MQTT C++ system-wide
find_package(PahoMqttCpp QUIET)

if(NOT PahoMqttCpp_FOUND)
    message(STATUS "Paho MQTT C++ not found. Fetching and building from source...")

    # Fetch and build Paho MQTT C++ (which includes the C library)
    FetchContent_Declare(
        paho_mqtt_cpp
        GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
        GIT_TAG v1.5.4  # Use the recommended version
    )
    # Initialize and update submodules (required for Paho MQTT C)
    FetchContent_GetProperties(paho_mqtt_cpp)
    if(NOT paho_mqtt_cpp_POPULATED)
        FetchContent_Populate(paho_mqtt_cpp)
        execute_process(
            COMMAND git submodule init
            WORKING_DIRECTORY ${paho_mqtt_cpp_SOURCE_DIR}
        )
        execute_process(
            COMMAND git submodule update
            WORKING_DIRECTORY ${paho_mqtt_cpp_SOURCE_DIR}
        )
    endif()

    # Build Paho MQTT C++ and C
    set(PAHO_WITH_MQTT_C ON CACHE BOOL "Build Paho C library with C++ library")
    set(PAHO_BUILD_EXAMPLES OFF CACHE BOOL "Do not build Paho examples")
    add_subdirectory(${paho_mqtt_cpp_SOURCE_DIR} ${paho_mqtt_cpp_BINARY_DIR})

    # Set variables for Paho MQTT C++
    set(PAHO_MQTT_CPP_LIBRARIES paho-mqttpp3)
    set(PAHO_MQTT_CPP_INCLUDE_DIRS ${paho_mqtt_cpp_SOURCE_DIR}/src)
else()
    message(STATUS "Paho MQTT C++ found: ${PahoMqttCpp_LIBRARIES}")
endif()

find_package(Curses QUIET)
if(NOT CURSES_FOUND)
    message(WARNING "ncurses library not found. Attempting to install...")
    execute_process(COMMAND bash -c "sudo apt update && sudo apt install -y libncurses5-dev libncursesw5-dev" RESULT_VARIABLE NCURSES_RESULT)
    if(NOT NCURSES_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install ncurses library. Please install manually.")
    endif()
endif()
# Ensure we have a found package and the standard variable names
find_package(Curses REQUIRED)

include_directories(include ${PAHO_MQTT_C_INCLUDE_DIR} ${PAHO_MQTT_CPP_INCLUDE_DIR})

add_executable(mqtt_console
    src/main.cpp
    src/ui.cpp
    src/mqtt.cpp
)

target_link_libraries(mqtt_console
    PRIVATE
    ${PAHO_MQTT_CPP_LIBRARIES}
    ${CURSES_LIBRARIES}
    # Fallback to system-installed paho libs if the FetchContent path didn't create an imported target
    paho-mqttpp3
    paho-mqtt3a
)
find_package(Threads REQUIRED)
target_link_libraries(mqtt_console PRIVATE Threads::Threads)
# Include directories
target_include_directories(mqtt_console
    PRIVATE
        ${PAHO_MQTT_CPP_INCLUDE_DIRS}
)

## nlohmann/json: prefer system package, fallback to FetchContent
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found system-wide: Fetching via FetchContent...")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_GetProperties(nlohmann_json)
    if(NOT nlohmann_json_POPULATED)
        FetchContent_Populate(nlohmann_json)
        add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR})
    endif()
endif()

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(mqtt_console PRIVATE nlohmann_json::nlohmann_json)
endif()

    # Copy example/default config into the build directory so the binary can read it at runtime
    if(EXISTS "${CMAKE_SOURCE_DIR}/client_config.json")
        configure_file(${CMAKE_SOURCE_DIR}/client_config.json ${CMAKE_BINARY_DIR}/client_config.json COPYONLY)
    endif()
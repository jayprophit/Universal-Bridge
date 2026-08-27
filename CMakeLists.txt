cmake_minimum_required(VERSION 3.20)
project(universal_bridge VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ubridge_core STATIC
    src/core/bridge_core.cpp
    src/core/session_tools.cpp
    src/core/performance_tools.cpp
)
target_include_directories(ubridge_core PUBLIC include)

add_library(ubridge_modules STATIC
    src/adapters/bridge_modules.cpp
    src/platform/local_service.cpp
    src/orchestrator/reporting_tools.cpp
)
target_include_directories(ubridge_modules PUBLIC include)
target_link_libraries(ubridge_modules PUBLIC ubridge_core)

add_executable(ubridge
    src/main.cpp
)
target_link_libraries(ubridge PRIVATE ubridge_core ubridge_modules)

add_executable(ubridge_test_lab
    src/test_lab/test_lab_main.cpp
)
target_link_libraries(ubridge_test_lab PRIVATE ubridge_core ubridge_modules)
target_include_directories(ubridge PRIVATE include)
target_compile_definitions(ubridge PRIVATE
    UBRIDGE_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(ubridge_core PRIVATE /W4 /permissive-)
    target_compile_options(ubridge_modules PRIVATE /W4 /permissive-)
    target_compile_options(ubridge_test_lab PRIVATE /W4 /permissive-)
    target_compile_options(ubridge PRIVATE /W4 /permissive-)
else()
    target_compile_options(ubridge_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_modules PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_test_lab PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

enable_testing()
add_test(NAME virtual_hardware_laboratory COMMAND ubridge_test_lab)

add_test(
    NAME bridge_demo_preflight
    COMMAND ubridge preflight
        --project ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mpc_sample_demo
        --daw cubase
        --output ${CMAKE_CURRENT_BINARY_DIR}/test-output
        --no-backup
)
set_tests_properties(bridge_demo_preflight PROPERTIES
    PASS_REGULAR_EXPRESSION "Preflight completed safely"
)

add_test(
    NAME bridge_invalid_daw
    COMMAND ubridge preflight
        --project ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mpc_sample_demo
        --daw unsupported
        --output ${CMAKE_CURRENT_BINARY_DIR}/invalid-output
)
set_tests_properties(bridge_invalid_daw PROPERTIES WILL_FAIL TRUE)

install(TARGETS ubridge ubridge_test_lab RUNTIME DESTINATION bin)
install(DIRECTORY profiles/ DESTINATION share/universal-bridge/profiles)
install(DIRECTORY docs/ DESTINATION share/doc/universal-bridge)

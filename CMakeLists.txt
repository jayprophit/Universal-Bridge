cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0091 NEW)
project(UniversalBridge VERSION 0.5.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

option(UBRIDGE_BUILD_TESTS "Build deterministic offline tests" ON)
option(UBRIDGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(UBRIDGE_STATIC_MSVC_RUNTIME "Statically link the MSVC runtime for portable ZIP packages" ON)

if(MSVC AND UBRIDGE_STATIC_MSVC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ubridge_core STATIC
    src/core/bridge_core.cpp
    src/core/protocol_capabilities.cpp
    src/core/session_tools.cpp
    src/core/performance_tools.cpp
    src/core/sync_guard.cpp
)
target_include_directories(ubridge_core PUBLIC include)
target_compile_features(ubridge_core PUBLIC cxx_std_20)

add_library(ubridge_modules STATIC
    src/adapters/bridge_modules.cpp
    src/platform/local_service.cpp
    src/platform/hardware_backends.cpp
    src/orchestrator/reporting_tools.cpp
)
target_include_directories(ubridge_modules PUBLIC include)
target_link_libraries(ubridge_modules PUBLIC ubridge_core)
target_compile_features(ubridge_modules PUBLIC cxx_std_20)

if(WIN32)
    target_link_libraries(ubridge_modules PRIVATE cfgmgr32)
endif()

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

foreach(target IN ITEMS ubridge_core ubridge_modules ubridge ubridge_test_lab)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endforeach()

if(MSVC)
    target_compile_options(ubridge_core PRIVATE /W4 /permissive-)
    target_compile_options(ubridge_modules PRIVATE /W4 /permissive-)
    target_compile_options(ubridge_test_lab PRIVATE /W4 /permissive-)
    target_compile_options(ubridge PRIVATE /W4 /permissive-)
    if(UBRIDGE_WARNINGS_AS_ERRORS)
        foreach(target IN ITEMS ubridge_core ubridge_modules ubridge_test_lab ubridge)
            target_compile_options(${target} PRIVATE /WX)
        endforeach()
    endif()
else()
    target_compile_options(ubridge_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_modules PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_test_lab PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(UBRIDGE_WARNINGS_AS_ERRORS)
        foreach(target IN ITEMS ubridge_core ubridge_modules ubridge_test_lab ubridge)
            target_compile_options(${target} PRIVATE -Werror)
        endforeach()
    endif()
endif()

if(BUILD_TESTING AND UBRIDGE_BUILD_TESTS)
add_test(NAME virtual_hardware_laboratory COMMAND ubridge_test_lab)
add_test(NAME read_only_device_inventory COMMAND ubridge devices)
set_tests_properties(read_only_device_inventory PROPERTIES PASS_REGULAR_EXPRESSION "No interfaces were opened")

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
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME generated_output_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_outputs.py)
    set_tests_properties(generated_output_validation PROPERTIES ENVIRONMENT "UBRIDGE_BINARY=$<TARGET_FILE:ubridge>")
endif()
endif()

install(TARGETS ubridge RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY profiles/ DESTINATION ${CMAKE_INSTALL_DATADIR}/universal-bridge/profiles)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(CPACK_PACKAGE_VENDOR "Universal Bridge")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Capability-aware hardware session bridge foundation")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "ZIP")
include(CPack)

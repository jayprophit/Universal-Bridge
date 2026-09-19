cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0091 NEW)
project(UniversalBridge VERSION 0.5.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)
include(FetchContent)
find_package(Threads REQUIRED)

option(UBRIDGE_BUILD_TESTS "Build deterministic offline tests" ON)
option(UBRIDGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(UBRIDGE_STATIC_MSVC_RUNTIME "Statically link the MSVC runtime for portable ZIP packages" ON)
option(UBRIDGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer on supported compilers" OFF)
option(UBRIDGE_BUILD_WINDOWS_MSI "Add an unsigned per-user WiX MSI to CPack output" OFF)
option(UBRIDGE_BUILD_VST3_CLIENT "Build the experimental, audio-transparent VST3 client" OFF)
set(UBRIDGE_WIX_ROOT "" CACHE PATH "Optional directory containing the WiX v4 command-line tool")
set(UBRIDGE_VST3_SDK_ROOT "" CACHE PATH "Path to a separately obtained Steinberg VST3 SDK checkout")

if(MSVC AND UBRIDGE_ENABLE_SANITIZERS)
    # Clang-CL AddressSanitizer cannot link the static MSVC debug runtime.
    # It also rejects the debug DLL runtime, so keep sanitizer builds isolated
    # on the non-debug dynamic runtime while retaining debug symbols.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
elseif(MSVC AND UBRIDGE_STATIC_MSVC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

FetchContent_Declare(zlib
    URL https://github.com/madler/zlib/archive/refs/tags/v1.3.2.tar.gz
    URL_HASH SHA256=b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)
FetchContent_MakeAvailable(nlohmann_json)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ubridge_core STATIC
    src/core/bridge_core.cpp
    src/core/midi_mediation.cpp
    src/core/parameter_sync.cpp
    src/core/protocol_capabilities.cpp
    src/core/state_sync.cpp
    src/core/session_tools.cpp
    src/core/session_store.cpp
    src/core/performance_tools.cpp
    src/core/sync_guard.cpp
    src/core/mpc_xpj_reader.cpp
)
target_include_directories(ubridge_core PUBLIC include)
target_compile_features(ubridge_core PUBLIC cxx_std_20)
target_link_libraries(ubridge_core PUBLIC zlibstatic nlohmann_json::nlohmann_json)

add_library(ubridge_modules STATIC
    src/adapters/bridge_modules.cpp
    src/platform/local_ipc.cpp
    src/platform/local_service.cpp
    src/platform/hardware_backends.cpp
    src/orchestrator/reporting_tools.cpp
)
target_include_directories(ubridge_modules PUBLIC include)
target_link_libraries(ubridge_modules PUBLIC ubridge_core)
target_compile_features(ubridge_modules PUBLIC cxx_std_20)

if(WIN32)
    target_link_libraries(ubridge_modules PRIVATE advapi32 bcrypt cfgmgr32 crypt32 winmm ole32 uuid)
endif()

add_executable(ubridge
    src/main.cpp
)
target_link_libraries(ubridge PRIVATE ubridge_core ubridge_modules)

add_executable(ubridge_test_lab
    src/test_lab/test_lab_main.cpp
)
add_executable(ubridge_service
    src/service/service_main.cpp
)
add_executable(ubridge_soak
    src/test_lab/soak_main.cpp
)
target_link_libraries(ubridge_service PRIVATE ubridge_core ubridge_modules)
target_link_libraries(ubridge_soak PRIVATE ubridge_core Threads::Threads)
target_include_directories(ubridge_service PRIVATE include)
target_link_libraries(ubridge_test_lab PRIVATE ubridge_core ubridge_modules)
target_include_directories(ubridge PRIVATE include)
target_compile_definitions(ubridge PRIVATE
    UBRIDGE_VERSION="${PROJECT_VERSION}"
)

foreach(target IN ITEMS ubridge_core ubridge_modules ubridge ubridge_test_lab ubridge_service ubridge_soak)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endforeach()

if(UBRIDGE_BUILD_VST3_CLIENT)
    if(CMAKE_VERSION VERSION_LESS 3.25)
        message(FATAL_ERROR "The supported VST3 SDK route requires CMake 3.25 or newer")
    endif()
    if(NOT IS_DIRECTORY "${UBRIDGE_VST3_SDK_ROOT}/public.sdk" OR
       NOT EXISTS "${UBRIDGE_VST3_SDK_ROOT}/LICENSE.txt")
        message(FATAL_ERROR
            "UBRIDGE_VST3_SDK_ROOT must identify a complete, separately obtained VST3 SDK checkout")
    endif()

    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
    add_subdirectory("${UBRIDGE_VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3-sdk" EXCLUDE_FROM_ALL)

    smtg_add_vst3plugin(UniversalBridgeVST3
        src/plugins/vst3/bridge_controller.cpp
        src/plugins/vst3/bridge_controller.hpp
        src/plugins/vst3/bridge_ids.hpp
        src/plugins/vst3/bridge_processor.cpp
        src/plugins/vst3/bridge_processor.hpp
        src/plugins/vst3/factory.cpp
    )
    target_compile_features(UniversalBridgeVST3 PRIVATE cxx_std_20)
    target_include_directories(UniversalBridgeVST3 PRIVATE src/plugins/vst3)
    target_link_libraries(UniversalBridgeVST3 PRIVATE sdk)
    set_target_properties(UniversalBridgeVST3 PROPERTIES FOLDER "Universal Bridge/Experimental")

    if(MSVC)
        # The VST3 SDK and host ABI use the DLL runtime; the standalone ZIP executables
        # retain the project's independent static-runtime portability policy.
        set_property(TARGET UniversalBridgeVST3 PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        # C4458 and C4996 originate in the pinned SDK's constexpr factory header.
        # Keep /W4 and /WX for Universal Bridge code while isolating reviewed third-party warnings.
        target_compile_options(UniversalBridgeVST3 PRIVATE /W4 /permissive- /wd4458 /wd4996)
        if(UBRIDGE_WARNINGS_AS_ERRORS)
            target_compile_options(UniversalBridgeVST3 PRIVATE /WX)
        endif()
    else()
        target_compile_options(UniversalBridgeVST3 PRIVATE -Wall -Wextra -Wpedantic)
        if(UBRIDGE_WARNINGS_AS_ERRORS)
            target_compile_options(UniversalBridgeVST3 PRIVATE -Werror)
        endif()
    endif()
endif()

if(UBRIDGE_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        foreach(target IN ITEMS ubridge_core ubridge_modules ubridge_test_lab ubridge ubridge_service ubridge_soak)
            if(MSVC)
                # Clang-CL/MSVC-like Windows builds provide AddressSanitizer but not UBSan.
                target_compile_options(${target} PRIVATE /fsanitize=address /Oy-)
            else()
                target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(${target} PRIVATE -fsanitize=address,undefined)
            endif()
        endforeach()

        if(MSVC)
            execute_process(
                COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
                OUTPUT_VARIABLE UBRIDGE_CLANG_RESOURCE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
            set(UBRIDGE_ASAN_RUNTIME_DIR "${UBRIDGE_CLANG_RESOURCE_DIR}/lib/windows")
            set(UBRIDGE_ASAN_DYNAMIC "${UBRIDGE_ASAN_RUNTIME_DIR}/clang_rt.asan_dynamic-x86_64.lib")
            set(UBRIDGE_ASAN_THUNK "${UBRIDGE_ASAN_RUNTIME_DIR}/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")
            set(UBRIDGE_ASAN_DLL "${UBRIDGE_ASAN_RUNTIME_DIR}/clang_rt.asan_dynamic-x86_64.dll")
            foreach(runtime_file IN ITEMS "${UBRIDGE_ASAN_DYNAMIC}" "${UBRIDGE_ASAN_THUNK}" "${UBRIDGE_ASAN_DLL}")
                if(NOT EXISTS "${runtime_file}")
                    message(FATAL_ERROR "Required Clang-CL AddressSanitizer runtime is missing: ${runtime_file}")
                endif()
            endforeach()
            foreach(target IN ITEMS ubridge_test_lab ubridge ubridge_service ubridge_soak)
                target_link_libraries(${target} PRIVATE "${UBRIDGE_ASAN_DYNAMIC}")
                target_link_options(${target} PRIVATE
                    "/include:__asan_seh_interceptor"
                    "/wholearchive:${UBRIDGE_ASAN_THUNK}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${UBRIDGE_ASAN_DLL}" "$<TARGET_FILE_DIR:${target}>")
            endforeach()
        endif()
    else()
        message(FATAL_ERROR "UBRIDGE_ENABLE_SANITIZERS requires a GCC- or Clang-compatible compiler")
    endif()
endif()

if(MSVC)
    # C4324 reports the deliberate cache-line padding on lock-free queues.
    target_compile_options(ubridge_core PRIVATE /W4 /permissive- /wd4324)
    target_compile_options(ubridge_modules PRIVATE /W4 /permissive- /wd4324)
    target_compile_options(ubridge_test_lab PRIVATE /W4 /permissive- /wd4324)
    target_compile_options(ubridge PRIVATE /W4 /permissive- /wd4324)
    target_compile_options(ubridge_service PRIVATE /W4 /permissive- /wd4324)
    target_compile_options(ubridge_soak PRIVATE /W4 /permissive- /wd4324)
    if(UBRIDGE_WARNINGS_AS_ERRORS)
        foreach(target IN ITEMS ubridge_core ubridge_modules ubridge_test_lab ubridge ubridge_service ubridge_soak)
            target_compile_options(${target} PRIVATE /WX)
        endforeach()
    endif()
else()
    target_compile_options(ubridge_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_modules PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_test_lab PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_service PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(ubridge_soak PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(UBRIDGE_WARNINGS_AS_ERRORS)
        foreach(target IN ITEMS ubridge_core ubridge_modules ubridge_test_lab ubridge ubridge_service ubridge_soak)
            target_compile_options(${target} PRIVATE -Werror)
        endforeach()
    endif()
endif()

if(BUILD_TESTING AND UBRIDGE_BUILD_TESTS)
add_test(NAME virtual_hardware_laboratory COMMAND ubridge_test_lab)
add_test(NAME deterministic_core_stress COMMAND ubridge_soak)
set_tests_properties(deterministic_core_stress PROPERTIES
    PASS_REGULAR_EXPRESSION "deterministic stress passed"
    TIMEOUT 30)
add_test(NAME read_only_device_inventory COMMAND ubridge devices)
set_tests_properties(read_only_device_inventory PROPERTIES PASS_REGULAR_EXPRESSION "No interfaces were opened")
add_test(NAME service_diagnostic_startup COMMAND ubridge_service --diagnose-once)
set_tests_properties(service_diagnostic_startup PROPERTIES PASS_REGULAR_EXPRESSION "Universal Bridge per-user service diagnostic complete")
if(WIN32)
    add_test(NAME authenticated_local_ipc COMMAND ubridge_service --ipc-self-test ${CMAKE_CURRENT_BINARY_DIR}/test-ipc)
    set_tests_properties(authenticated_local_ipc PROPERTIES PASS_REGULAR_EXPRESSION "authenticated IPC self-test passed" TIMEOUT 20)
endif()

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
    add_test(NAME research_catalog_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_research_catalog.py --check)
    add_test(NAME repository_asset_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_repository_assets.py)
    add_test(NAME release_gate_manifest_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_release_readiness.py --check)
    add_custom_target(research_catalog
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_research_catalog.py --output ${CMAKE_CURRENT_BINARY_DIR}/research/poietek-catalog.sqlite3
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building local Poietek research catalog")
endif()
endif()

install(TARGETS ubridge ubridge_service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY profiles/ DESTINATION ${CMAKE_INSTALL_DATADIR}/universal-bridge/profiles)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY research/ DESTINATION ${CMAKE_INSTALL_DATADIR}/universal-bridge/research)
install(DIRECTORY licenses/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/licenses)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES SECURITY.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES "${zlib_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DOCDIR}/licenses/third-party RENAME zlib-LICENSE.txt)
install(FILES "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT" DESTINATION ${CMAKE_INSTALL_DOCDIR}/licenses/third-party RENAME nlohmann-json-LICENSE.MIT.txt)

set(CPACK_PACKAGE_VENDOR "Universal Bridge")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Capability-aware hardware session bridge foundation")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "Universal-Bridge-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "ZIP")
if(WIN32 AND UBRIDGE_BUILD_WINDOWS_MSI)
    list(APPEND CPACK_GENERATOR "WIX")
    set(CPACK_WIX_VERSION 4)
    set(CPACK_WIX_UPGRADE_GUID "093C1769-3915-446B-8911-46E7F1FF9E5B")
    set(CPACK_WIX_INSTALL_SCOPE perUser)
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/PRE_RELEASE_NOTICE.rtf")
    set(CPACK_WIX_PROPERTY_ARPCOMMENTS "Unsigned Universal Bridge pre-release development build")
    set(CPACK_WIX_PROPERTY_ARPHELPLINK "https://github.com/jayprophit/Universal-Bridge/issues")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/jayprophit/Universal-Bridge")
    if(UBRIDGE_WIX_ROOT)
        set(CPACK_WIX_ROOT "${UBRIDGE_WIX_ROOT}")
    endif()
endif()
include(CPack)

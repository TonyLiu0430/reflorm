cmake_minimum_required(VERSION 3.25)

project(cpporm
    VERSION 0.1.0
    DESCRIPTION "Header-only C++26 ORM"
    LANGUAGES C CXX
)

if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 26)
    message(FATAL_ERROR "cpporm requires C++26 or newer; CMAKE_CXX_STANDARD is ${CMAKE_CXX_STANDARD}.")
endif()

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 26 CACHE STRING "C++ standard to use")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json")
set(CPPORM_RUNTIME_DLL_DIR "" CACHE PATH "Directory containing the C++ runtime DLLs used by tests")

add_library(cpporm INTERFACE)
add_library(cpporm::cpporm ALIAS cpporm)

target_compile_features(cpporm INTERFACE cxx_std_26)

target_include_directories(cpporm
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(cpporm
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU>:-freflection>
)

option(CPPORM_BUILD_TESTS "Build cpporm tests" ${PROJECT_IS_TOP_LEVEL})
option(CPPORM_ENABLE_SQLITE_RUNTIME "Build optional SQLite runtime backend" OFF)

if(CPPORM_ENABLE_SQLITE_RUNTIME)
    include(FetchContent)

    FetchContent_Declare(
        sqlite_amalgamation
        URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip
    )
    FetchContent_MakeAvailable(sqlite_amalgamation)

    add_library(cpporm_sqlite_runtime STATIC
        ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c
    )
    add_library(cpporm::sqlite_runtime ALIAS cpporm_sqlite_runtime)

    target_include_directories(cpporm_sqlite_runtime
        PUBLIC
            ${sqlite_amalgamation_SOURCE_DIR}
    )
    target_compile_options(cpporm_sqlite_runtime
        PRIVATE
            $<$<C_COMPILER_ID:GNU>:-w>
    )
    target_link_libraries(cpporm_sqlite_runtime
        INTERFACE
            cpporm::cpporm
    )
endif()

if(CPPORM_BUILD_TESTS)
    include(CTest)

    if(BUILD_TESTING)
        include(FetchContent)

        FetchContent_Declare(
            doctest
            URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.zip
            URL_HASH SHA256=7a7afb5f70d0b749d49ddfcb8a454299a8fcd53e9db9c131abe99b456e88a1fe
        )
        FetchContent_MakeAvailable(doctest)

        add_executable(cpporm_tests
            tests/basic_mapping_tests.cpp
            tests/join_tests.cpp
            tests/mutation_tests.cpp
            tests/nested_select_tests.cpp
            tests/predicate_tests.cpp
            tests/relation_tests.cpp
            tests/relationship_tests.cpp
            tests/schema_tests.cpp
        )
        target_link_libraries(cpporm_tests
            PRIVATE
                cpporm::cpporm
                doctest::doctest
        )
        # GCC 16 reflection is experimental; optimized doctest binaries can hang
        # while checking reflected metadata strings in this snapshot.
        target_compile_options(cpporm_tests PRIVATE $<$<CXX_COMPILER_ID:GNU>:-O0>)

        add_test(NAME cpporm_tests COMMAND cpporm_tests)

        if(CPPORM_ENABLE_SQLITE_RUNTIME)
            add_executable(cpporm_sqlite_runtime_tests
                tests/sqlite_runtime_tests.cpp
            )
            target_link_libraries(cpporm_sqlite_runtime_tests
                PRIVATE
                    cpporm::sqlite_runtime
                    doctest::doctest
            )
            target_compile_options(cpporm_sqlite_runtime_tests PRIVATE $<$<CXX_COMPILER_ID:GNU>:-O0>)
            add_test(NAME cpporm_sqlite_runtime_tests COMMAND cpporm_sqlite_runtime_tests)
        endif()

        function(add_cpporm_compile_fail_test test_name source_file expected_message)
            set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${test_name}.obj")
            set(reflection_flag "")
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                set(reflection_flag -freflection)
            endif()

            add_test(
                NAME ${test_name}
                COMMAND
                    ${CMAKE_COMMAND}
                    -DCPPORM_TEST_CXX=${CMAKE_CXX_COMPILER}
                    -DCPPORM_TEST_REFLECTION_FLAG=${reflection_flag}
                    -DCPPORM_TEST_INCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/include
                    -DCPPORM_TEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source_file}
                    -DCPPORM_TEST_OUTPUT=${output_file}
                    -DCPPORM_TEST_EXPECTED=${expected_message}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_compile_failure.cmake
            )
        endfunction()

        add_cpporm_compile_fail_test(
            cpporm_compile_fail_reference_to_unregistered_model
            tests/compile_fail/reference_to_unregistered_model.cpp
            "cpporm referenced model must be registered in the same namespace"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_reference_to_ignored_field
            tests/compile_fail/reference_to_ignored_field.cpp
            "cpporm referenced field must not be ignored"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_reference_type_mismatch
            tests/compile_fail/reference_type_mismatch.cpp
            "cpporm reference field type must match referenced field type"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_join_without_reference
            tests/compile_fail/join_without_reference.cpp
            "cpporm join target has no reference from source model"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_join_ambiguous_reference
            tests/compile_fail/join_ambiguous_reference.cpp
            "cpporm join target is ambiguous"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_relation_without_reference
            tests/compile_fail/relation_without_reference.cpp
            "cpporm relation target has no reference from source model"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_has_many_without_inverse_reference
            tests/compile_fail/has_many_without_inverse_reference.cpp
            "cpporm has_many target has no reference back to source model"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_nested_select_wrong_target_field
            tests/compile_fail/nested_select_wrong_target_field.cpp
            "cpporm selected field does not belong to model"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_nested_select_without_root_field
            tests/compile_fail/nested_select_without_root_field.cpp
            "cpporm find_many select requires at least one root field"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_update_payload_without_primary_key
            tests/compile_fail/update_payload_without_primary_key.cpp
            "cpporm update payload must contain primary key"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_upsert_payload_without_update_fields
            tests/compile_fail/upsert_payload_without_update_fields.cpp
            "cpporm upsert payload must contain at least one non-primary-key model field"
        )
        add_cpporm_compile_fail_test(
            cpporm_compile_fail_model_constraint_must_be_static
            tests/compile_fail/model_constraint_must_be_static.cpp
            "cpporm model_constraint must be static constexpr"
        )

        if(CPPORM_RUNTIME_DLL_DIR)
            set_tests_properties(cpporm_tests PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CPPORM_RUNTIME_DLL_DIR}"
            )
            if(CPPORM_ENABLE_SQLITE_RUNTIME)
                set_tests_properties(cpporm_sqlite_runtime_tests PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CPPORM_RUNTIME_DLL_DIR}"
                )
            endif()
        endif()
    endif()
endif()

cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" IEUM_VERSION LIMIT_COUNT 1)
string(STRIP "${IEUM_VERSION}" IEUM_VERSION)
if(NOT IEUM_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version such as 0.1.0")
endif()
set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
)

project(ieum
    VERSION ${IEUM_VERSION}
    DESCRIPTION "Structural dependency checker for Ieum source files"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IEUM_HEADERS
    src/token.h
    src/ast.h
    src/lexer.h
    src/parser.h
    src/checker.h
    src/graph.h
    src/semantic.h
    src/interpreter.h
    src/version.h
)

function(add_ieum_executable target source)
    add_executable(${target} ${source} ${IEUM_HEADERS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(${target} PRIVATE IEUM_VERSION="${PROJECT_VERSION}")

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

add_ieum_executable(ieum src/main.cpp)
add_ieum_executable(testParser test/testParser.cpp)
add_ieum_executable(testPipeline test/testPipeline.cpp)
add_ieum_executable(testChecker test/testChecker.cpp)
add_ieum_executable(testGraph test/testGraph.cpp)
add_ieum_executable(testSemantic test/testSemantic.cpp)
add_ieum_executable(testInterpreter test/testInterpreter.cpp)
add_ieum_executable(benchmarkChecker benchmark/benchmarkChecker.cpp)

if(MSVC)
    target_compile_options(benchmarkChecker PRIVATE /O2)
else()
    target_compile_options(benchmarkChecker PRIVATE -O2)
endif()

enable_testing()

add_test(NAME parser COMMAND testParser)
add_test(NAME pipeline COMMAND testPipeline)
add_test(NAME checker COMMAND testChecker)
add_test(NAME graph COMMAND testGraph)
add_test(NAME semantic COMMAND testSemantic)
add_test(NAME interpreter COMMAND testInterpreter)
add_test(NAME benchmark_smoke COMMAND benchmarkChecker 2 1)
set_tests_properties(benchmark_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "scenario=layered-valid-chain"
)

add_test(NAME cli_version COMMAND $<TARGET_FILE:ieum> --version)
set_tests_properties(cli_version PROPERTIES
    PASS_REGULAR_EXPRESSION "ieum ${PROJECT_VERSION}"
)

function(add_ieum_example_test name expected_exit expected_text)
    add_test(
        NAME example_${name}
        COMMAND ${CMAKE_COMMAND}
            "-DIEUM_EXECUTABLE=$<TARGET_FILE:ieum>"
            "-DIEUM_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/${name}.ieum"
            "-DEXPECTED_EXIT=${expected_exit}"
            "-DEXPECTED_TEXT=${expected_text}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyExample.cmake"
    )
endfunction()

function(add_ieum_run_test test_name source_name entry expected_exit expected_text)
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            "-DIEUM_EXECUTABLE=$<TARGET_FILE:ieum>"
            "-DIEUM_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/${source_name}.ieum"
            "-DIEUM_ENTRY=${entry}"
            "-DEXPECTED_EXIT=${expected_exit}"
            "-DEXPECTED_TEXT=${expected_text}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyExample.cmake"
    )
endfunction()

add_ieum_example_test(valid 0 "")
add_ieum_example_test(module_body 0 "body_modules=2")
add_ieum_run_test(example_execution execution "service.main" 0 "calls_executed=2")
add_ieum_run_test(cli_run_missing_function execution "service.missing" 1 "service.missing")
add_ieum_run_test(cli_run_parameterized_entry execution "service.prepare" 1 "service.prepare")
add_ieum_run_test(cli_run_invalid_entry execution "service" 2 "entry_format=module.function")
add_ieum_example_test(implicit_dependency 1 "notification")
add_ieum_example_test(cyclic_dependency 1 "order -> payment -> order")
add_ieum_example_test(layer_violation 1 "'data'")
add_ieum_example_test(transitive_layer_violation 1 "'data'")
add_ieum_example_test(invalid_declarations 1 "'missing'")
add_ieum_example_test(semantic_undefined_function 1 "missing")
add_ieum_example_test(semantic_arity_mismatch 1 "helper")
add_ieum_example_test(semantic_missing_dependency 1 "depends")
add_ieum_example_test(semantic_undefined_variable 1 "missing")

function(add_ieum_graph_test name expected_exit)
    add_test(
        NAME graph_snapshot_${name}
        COMMAND ${CMAKE_COMMAND}
            "-DIEUM_EXECUTABLE=$<TARGET_FILE:ieum>"
            "-DIEUM_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/${name}.ieum"
            "-DACTUAL_DOT=${CMAKE_CURRENT_BINARY_DIR}/graph-${name}.dot"
            "-DEXPECTED_DOT=${CMAKE_CURRENT_SOURCE_DIR}/test/snapshots/${name}.dot"
            "-DEXPECTED_EXIT=${expected_exit}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyGraph.cmake"
    )
endfunction()

add_ieum_graph_test(valid 0)
add_ieum_graph_test(implicit_dependency 1)
add_ieum_graph_test(cyclic_dependency 1)
add_ieum_graph_test(layer_violation 1)
add_ieum_graph_test(transitive_layer_violation 1)
add_ieum_graph_test(invalid_declarations 1)

function(add_ieum_graph_failure_test name source expected_exit)
    add_test(
        NAME graph_failure_${name}
        COMMAND ${CMAKE_COMMAND}
            "-DIEUM_EXECUTABLE=$<TARGET_FILE:ieum>"
            "-DIEUM_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/${source}.ieum"
            "-DOUTPUT_TARGET=${CMAKE_CURRENT_BINARY_DIR}"
            "-DEXPECTED_EXIT=${expected_exit}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyGraphFailure.cmake"
    )
endfunction()

add_ieum_graph_failure_test(valid valid 0)
add_ieum_graph_failure_test(invalid cyclic_dependency 1)

# Python is optional for the core. CI also exercises the demo/evaluation tools.
find_package(Python3 3.9 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME workflow_tools COMMAND ${Python3_EXECUTABLE} -B
        "${CMAKE_CURRENT_SOURCE_DIR}/test/test_workflows.py"
        --ieum $<TARGET_FILE:ieum>)
    add_test(NAME seed_evaluation COMMAND ${Python3_EXECUTABLE} -B
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/evaluate.py"
        --ieum $<TARGET_FILE:ieum>
        --output "${CMAKE_CURRENT_BINARY_DIR}/evaluation/results.json")
    add_test(NAME demo_replay COMMAND ${Python3_EXECUTABLE} -B
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/demo.py"
        --ieum $<TARGET_FILE:ieum>
        --output "${CMAKE_CURRENT_BINARY_DIR}/demo/results.json")
endif()

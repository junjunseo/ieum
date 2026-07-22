cmake_minimum_required(VERSION 3.16)

project(ieum
    VERSION 0.1.0
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
)

function(add_ieum_executable target source)
    add_executable(${target} ${source} ${IEUM_HEADERS})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

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

enable_testing()

add_test(NAME parser COMMAND testParser)
add_test(NAME pipeline COMMAND testPipeline)
add_test(NAME checker COMMAND testChecker)

add_test(
    NAME example_valid
    COMMAND $<TARGET_FILE:ieum> ${CMAKE_CURRENT_SOURCE_DIR}/examples/valid.ieum
)

set(IEUM_INVALID_EXAMPLES
    implicit_dependency
    cyclic_dependency
    layer_violation
    transitive_layer_violation
    invalid_declarations
)

foreach(example IN LISTS IEUM_INVALID_EXAMPLES)
    add_test(
        NAME example_${example}
        COMMAND $<TARGET_FILE:ieum> ${CMAKE_CURRENT_SOURCE_DIR}/examples/${example}.ieum
    )
    set_tests_properties(example_${example} PROPERTIES WILL_FAIL TRUE)
endforeach()

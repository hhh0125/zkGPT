add_library(lasso_prototype STATIC
    lasso_table.cpp lasso_commitment.cpp lasso_serialization.cpp
    lasso_sumcheck.cpp lasso_memory.cpp lasso_prover.cpp lasso_verifier.cpp)
target_include_directories(lasso_prototype PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lasso_prototype PUBLIC gpt_lib)
target_compile_options(lasso_prototype PRIVATE -Wall -Wextra -Wpedantic)
add_executable(lasso_verify lasso_verify_main.cpp)
target_link_libraries(lasso_verify PRIVATE lasso_prototype)
add_executable(lasso_tests tests/lasso_tests.cpp)
target_link_libraries(lasso_tests PRIVATE lasso_prototype)
add_test(NAME route_b_lasso_unit COMMAND lasso_tests ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
set_tests_properties(route_b_lasso_unit PROPERTIES TIMEOUT 180)
find_program(LASSO_PYTHON3 python3)
if(LASSO_PYTHON3)
    add_test(NAME route_b_lasso_independent
        COMMAND ${LASSO_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_independent.py
            $<TARGET_FILE:lasso_tests> $<TARGET_FILE:lasso_verify>
            ${CMAKE_CURRENT_BINARY_DIR}/independent-fixtures)
    set_tests_properties(route_b_lasso_independent PROPERTIES TIMEOUT 180)
endif()
option(ZKGPT_BUILD_LASSO_BINDING "Build standalone Route B B2 witness binding" OFF)
if(ZKGPT_BUILD_LASSO_BINDING)
    add_subdirectory(binding)
endif()

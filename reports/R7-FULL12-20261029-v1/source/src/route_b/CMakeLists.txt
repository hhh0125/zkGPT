add_library(route_b_production STATIC production_layout.cpp source_plan.cpp)
target_include_directories(route_b_production PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(route_b_production PUBLIC gpt_lib)
target_compile_options(route_b_production PRIVATE -Wall -Wextra -Wpedantic)

add_executable(route_b_source_plan_tests tests/source_plan_tests.cpp)
target_link_libraries(route_b_source_plan_tests PRIVATE route_b_production)
add_executable(route_b_source_plan_emit tests/source_plan_emit.cpp)
target_link_libraries(route_b_source_plan_emit PRIVATE route_b_production)
# Only the evidence emitter intercepts the pre-subset access stream. The
# production library and legacy targets have no linker wrapper or I/O hooks.
target_link_options(route_b_source_plan_emit PRIVATE -Wl,--wrap=_ZN14layeredCircuit10initSubsetEv)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME route_b_source_plan_unit COMMAND route_b_source_plan_tests)
add_test(NAME route_b_source_plan_independent_full12
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_plan_independent.py
    --emitter $<TARGET_FILE:route_b_source_plan_emit>
    --output ${CMAKE_BINARY_DIR}/source-plan-full12)
set_tests_properties(route_b_source_plan_unit route_b_source_plan_independent_full12
  PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 1800 RUN_SERIAL TRUE)

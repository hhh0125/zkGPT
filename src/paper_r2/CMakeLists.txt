add_library(paper_r2 STATIC r6_mapping.cpp protocol.cpp wire.cpp model_queries.cpp model_resources.cpp r5b_trust.cpp r6_model.cpp r6_trust.cpp)
target_include_directories(paper_r2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(paper_r2 PUBLIC gpt_lib paper_r1)
target_compile_options(paper_r2 PRIVATE -Wall -Wextra -Wpedantic)
add_executable(r2_integration_tests tests_integration.cpp)
target_link_libraries(r2_integration_tests PRIVATE paper_r2 gkr_test_support)
add_test(NAME r2_integration COMMAND r2_integration_tests)

add_executable(r2_model_probe model_probe.cpp)
target_link_libraries(r2_model_probe PRIVATE paper_r2)

add_library(paper_r2_verifier STATIC protocol.cpp wire.cpp model_queries.cpp model_resources.cpp r5b_trust.cpp r6_model.cpp r6_trust.cpp)
target_compile_definitions(paper_r2_verifier PRIVATE R1_VERIFY_ONLY=1)
target_link_libraries(paper_r2_verifier PUBLIC gpt_lib paper_r1_verifier)
target_include_directories(paper_r2_verifier PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(r2_verify verify_main.cpp)
target_link_libraries(r2_verify PRIVATE paper_r2_verifier)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME r2_independent COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/independent.py $<TARGET_FILE:r2_integration_tests> $<TARGET_FILE:r2_verify>)
set_tests_properties(r2_independent PROPERTIES TIMEOUT 240 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_executable(r2_model_queries_tests tests_model_queries.cpp)
target_link_libraries(r2_model_queries_tests PRIVATE paper_r2)
add_test(NAME r2_model_queries COMMAND r2_model_queries_tests)

add_executable(r2_model_source_probe model_source_probe.cpp)
target_link_libraries(r2_model_source_probe PRIVATE paper_r2)
add_executable(r2_model_source_verify model_source_verify.cpp)
target_link_libraries(r2_model_source_verify PRIVATE paper_r2_verifier)

add_executable(r2_model_integrated model_integrated.cpp)
target_link_libraries(r2_model_integrated PRIVATE paper_r2)
add_executable(r2_model_verify model_verify.cpp)
target_link_libraries(r2_model_verify PRIVATE paper_r2_verifier)

add_executable(r2_model_attack_artifacts model_attack_artifacts.cpp)
target_link_libraries(r2_model_attack_artifacts PRIVATE paper_r2)

add_executable(r3_model_resources_tests tests_model_resources.cpp)
target_link_libraries(r3_model_resources_tests PRIVATE paper_r2)
add_test(NAME r3_model_resources COMMAND r3_model_resources_tests)

add_executable(r3_model_plan model_plan.cpp)
target_link_libraries(r3_model_plan PRIVATE paper_r2)
add_executable(r3_model_semantics_probe model_semantics_probe.cpp)
target_link_libraries(r3_model_semantics_probe PRIVATE paper_r2)

add_executable(r3_progress_tests tests_progress.cpp)
target_link_libraries(r3_progress_tests PRIVATE paper_r2 gkr_test_support)
add_test(NAME r3_progress COMMAND r3_progress_tests)

add_executable(r4_numerical_tests tests_numerical_candidate.cpp)
target_link_libraries(r4_numerical_tests PRIVATE paper_r2)
add_test(NAME r4_numerical COMMAND r4_numerical_tests)

add_executable(r4_model_numerical_probe model_numerical_probe.cpp)
target_link_libraries(r4_model_numerical_probe PRIVATE paper_r2)

add_executable(r5b_trust_tests tests_r5b_trust.cpp)
target_link_libraries(r5b_trust_tests PRIVATE paper_r2)

add_executable(r5b_model_preprocess r5b_model_preprocess.cpp)
target_link_libraries(r5b_model_preprocess PRIVATE paper_r2)

add_executable(r5b_model_prove r5b_model_prove.cpp)
target_link_libraries(r5b_model_prove PRIVATE paper_r2)
add_executable(r5b_model_verify r5b_model_verify.cpp)
target_link_libraries(r5b_model_verify PRIVATE paper_r2_verifier)
add_test(NAME r5b_trusted_W COMMAND r5b_trust_tests)
add_test(NAME r5b_blocks12_guard COMMAND r5b_model_prove unused-r5b-artifacts --blocks 12)
set_tests_properties(r5b_blocks12_guard PROPERTIES WILL_FAIL TRUE)

add_executable(r5b_mapping_tests tests_r5b_mapping.cpp)
target_link_libraries(r5b_mapping_tests PRIVATE paper_r2)
add_test(NAME r5b_parameter_mapping COMMAND r5b_mapping_tests small)
set_tests_properties(r5b_parameter_mapping PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(r6_builder_tests tests_r6_builder.cpp)
target_link_libraries(r6_builder_tests PRIVATE paper_r2)

add_executable(r6_trust_tests tests_r6_trust.cpp)
target_link_libraries(r6_trust_tests PRIVATE paper_r2_verifier)

add_executable(r6_model_preprocess r6_model_preprocess.cpp)
target_link_libraries(r6_model_preprocess PRIVATE paper_r2)

add_executable(r6_witness_tests tests_r6_witness.cpp)
target_link_libraries(r6_witness_tests PRIVATE paper_r2)

add_executable(r6_model_plan r6_model_plan.cpp)
target_link_libraries(r6_model_plan PRIVATE paper_r2_verifier)

add_executable(r6_model_prove r6_model_prove.cpp)
target_link_libraries(r6_model_prove PRIVATE paper_r2)

add_executable(r6_model_verify r6_model_verify.cpp)
target_link_libraries(r6_model_verify PRIVATE paper_r2_verifier)

add_executable(r6_integration_tests tests_r6_integration.cpp)
target_link_libraries(r6_integration_tests PRIVATE paper_r2)

add_test(NAME r6_builder_oneblock COMMAND r6_builder_tests 1)
add_test(NAME r6_builder_full12 COMMAND r6_builder_tests 12)
add_test(NAME r6_trust_metadata COMMAND r6_trust_tests)
add_test(NAME r6_finite_threeblock COMMAND r6_witness_tests)
add_test(NAME r6_actual_schema COMMAND r6_integration_tests)
add_executable(r6_rowmajor_tests rowmajor_tests.cpp)
target_link_libraries(r6_rowmajor_tests PRIVATE paper_r2)
add_test(NAME r6_rowmajor_equivalence COMMAND r6_rowmajor_tests)
set_tests_properties(r6_rowmajor_equivalence PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180 ENVIRONMENT "ZKGPT_THREADS=4;MALLOC_ARENA_MAX=2")
add_test(NAME r6_entry_guards COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/r6_entry_tests.py $<TARGET_FILE:r6_model_prove> $<TARGET_FILE:r6_model_verify> $<TARGET_FILE:r6_model_plan>)
set_tests_properties(r6_builder_oneblock r6_builder_full12 r6_trust_metadata r6_finite_threeblock r6_actual_schema r6_entry_guards PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 300 ENVIRONMENT "ZKGPT_THREADS=4;MALLOC_ARENA_MAX=2")

add_executable(r6_resources_tests tests_r6_resources.cpp)
target_link_libraries(r6_resources_tests PRIVATE paper_r2)
add_test(NAME r6_system_resources COMMAND r6_resources_tests)

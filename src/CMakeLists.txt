# Full12 production build. Sources are explicitly listed; src has no subdirectories.

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

find_package(Threads REQUIRED)

add_library(gpt_lib STATIC
    circuit.cpp
    gkr_verifier.cpp
    hyrax.cpp
    hyrax_opening.cpp
    memory_diagnostics.cpp
    models.cpp
    neuralNetwork.cpp
    polynomial.cpp
    prover.cpp
    r5b_gelu.cpp
    r5b_layernorm.cpp
    r5b_operators.cpp
    timer.cpp
    utils.cpp
)

target_include_directories(gpt_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CNPY_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd/mcl/include)

add_library(paper_r1 STATIC
    paper_r1_core.cpp
    paper_r1_exp.cpp
    paper_r1_lookup_batch.cpp
    paper_r1_mapped_input.cpp
    paper_r1_memory.cpp
    paper_r1_pc.cpp
    paper_r1_range.cpp
    paper_r1_wire.cpp
)

target_include_directories(paper_r1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CNPY_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd/mcl/include)

add_library(paper_r2 STATIC
    paper_r2_model_queries.cpp
    paper_r2_model_resources.cpp
    paper_r2_r5b_trust.cpp
    paper_r2_r6_mapping.cpp
    paper_r2_r6_model.cpp
    paper_r2_r6_trust.cpp
)

target_include_directories(paper_r2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CNPY_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd/mcl/include)

add_library(paper_r7 STATIC
    paper_r7_category_auth.cpp
    paper_r7_category_commitments.cpp
    paper_r7_category_layout.cpp
    paper_r7_category_plan.cpp
    paper_r7_classified_commitments.cpp
    paper_r7_classified_layout.cpp
    paper_r7_crypto.cpp
    paper_r7_fixed_cw.cpp
    paper_r7_framework.cpp
    paper_r7_fs_io.cpp
    paper_r7_full12_model.cpp
    paper_r7_hyrax_a3.cpp
    paper_r7_lookup_prepare.cpp
    paper_r7_master_layout.cpp
    paper_r7_oneblock_gkr.cpp
    paper_r7_oneblock_lookup.cpp
    paper_r7_oneblock_model.cpp
    paper_r7_oneblock_plan.cpp
    paper_r7_oneblock_sumcheck.cpp
    paper_r7_oneblock_vectors.cpp
    paper_r7_opt2_metrics.cpp
    paper_r7_partitioned_layout.cpp
    paper_r7_public_k.cpp
    paper_r7_r6_adapter.cpp
    paper_r7_sigma.cpp
    paper_r7_single_commitments.cpp
    paper_r7_single_hyrax.cpp
    paper_r7_source_mle.cpp
    paper_r7_stage2_opening.cpp
    paper_r7_static_source_map.cpp
    paper_r7_streaming_master.cpp
    paper_r7_u_fold_state.cpp
    paper_r7_u_streaming.cpp
    paper_r7_unified_input.cpp
    paper_r7_wire.cpp
)

target_include_directories(paper_r7 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CNPY_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd/mcl/include)

target_link_libraries(gpt_lib PUBLIC ${CNPY_LIBRARY} mcl mclbn256 Threads::Threads z)

target_link_libraries(paper_r1 PUBLIC mcl mclbn256 Threads::Threads)

target_compile_options(paper_r1 PRIVATE -Wall -Wextra -Wpedantic -Werror)

target_link_libraries(paper_r2 PUBLIC gpt_lib paper_r1)

target_compile_options(paper_r2 PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(paper_r7 PUBLIC paper_r2 OpenSSL::Crypto)

target_compile_options(paper_r7 PRIVATE -Wall -Wextra -Werror)

add_executable(r7_full12_witness paper_r7_full12_witness_main.cpp)

target_link_libraries(r7_full12_witness PRIVATE paper_r7)

add_executable(r7_full12_preprocess paper_r7_full12_preprocess.cpp)

target_link_libraries(r7_full12_preprocess PRIVATE paper_r7)

add_executable(r7_full12_statement paper_r7_full12_statement.cpp)

target_link_libraries(r7_full12_statement PRIVATE paper_r7)

add_executable(r7_fs_full12_prover paper_r7_oneblock_main.cpp)

target_link_libraries(r7_fs_full12_prover PRIVATE paper_r7)

target_compile_definitions(r7_fs_full12_prover PRIVATE R7_CLASSIFIED_V2=1 R7_FS_ONEBLOCK=1 R7_FS_FULL12=1 R7_UNIFIED_INPUT1=1 R7_SINGLE_HYRAX=1)

add_executable(r7_fs_full12_verifier paper_r7_oneblock_main.cpp)

target_link_libraries(r7_fs_full12_verifier PRIVATE paper_r7)

target_compile_definitions(r7_fs_full12_verifier PRIVATE R7_CLASSIFIED_V2=1 R7_FS_ONEBLOCK=1 R7_FS_FULL12=1 R7_UNIFIED_INPUT1=1 R7_SINGLE_HYRAX=1 R7_COMPLETE_VERIFIER=1)

# Preserve the existing R7 treatment of shared production headers.
target_include_directories(paper_r7 SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

option(ZKGPT_KIND01_TESTS "Build focused Product01 protocol tests" OFF)
if(ZKGPT_KIND01_TESTS)
 add_executable(r7_kind01_tests paper_r7_kind01_tests.cpp)
 target_link_libraries(r7_kind01_tests PRIVATE paper_r7)
 target_compile_options(r7_kind01_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_kind01_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_product_polynomial_arithmetic COMMAND r7_kind01_tests arithmetic)
 add_test(NAME r7_perf6_config COMMAND r7_kind01_tests config)
 add_test(NAME r7_perf6_equivalence COMMAND r7_kind01_tests equivalence)
 add_test(NAME r7_kind01_shared COMMAND r7_kind01_tests shared 8 5)
 add_test(NAME r7_kind01_negative COMMAND r7_kind01_tests negative 8 5)
 add_test(NAME r7_kind01_length1 COMMAND r7_kind01_tests shared 1 1)
 add_test(NAME r7_kind01_length2 COMMAND r7_kind01_tests shared 2 1)
 add_test(NAME r7_kind01_length4 COMMAND r7_kind01_tests shared 4 3)
 add_test(NAME r7_kind01_length16 COMMAND r7_kind01_tests shared 16 9)
 add_test(NAME r7_kind01_range_integration COMMAND r7_kind01_tests range 8 5)
 add_test(NAME r7_kind01_exp_integration COMMAND r7_kind01_tests exp 4 3)
 # Optional archived two-file baseline, with the identical benchmark harness.
 # Never used by production targets or selected at protocol runtime.
 if(ZKGPT_KIND01_REFERENCE_DIR)
  add_executable(r7_kind01_reference_tests paper_r7_kind01_tests.cpp
   ${ZKGPT_KIND01_REFERENCE_DIR}/paper_r7_oneblock_lookup.cpp
   ${ZKGPT_KIND01_REFERENCE_DIR}/paper_r7_oneblock_sumcheck.cpp)
  target_compile_definitions(r7_kind01_reference_tests PRIVATE KIND01_REFERENCE=1)
  target_compile_options(r7_kind01_reference_tests PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(r7_kind01_reference_tests PRIVATE -Wl,--gc-sections)
  target_link_libraries(r7_kind01_reference_tests PRIVATE paper_r7)
 endif()
endif()

# Keep only code reachable from the supported Full12 entry points.
foreach(lib gpt_lib paper_r1 paper_r2 paper_r7)
 target_compile_options(${lib} PRIVATE -ffunction-sections -fdata-sections)
endforeach()
foreach(entry r7_full12_preprocess r7_full12_witness r7_full12_statement r7_fs_full12_prover r7_fs_full12_verifier)
 target_compile_options(${entry} PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(${entry} PRIVATE -Wl,--gc-sections "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${entry}.map")
endforeach()

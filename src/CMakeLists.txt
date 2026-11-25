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
    paper_r2_r6_model.cpp
    paper_r2_r6_trust.cpp
)

target_include_directories(paper_r2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CNPY_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd/mcl/include)

add_library(paper_r7 STATIC
    paper_r7_native_masked.cpp
    paper_r7_lasso_masked.cpp
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
 add_test(NAME r7_range_large_split COMMAND r7_kind01_tests rangesplit)
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

if(ZKGPT_KIND01_TESTS)
 add_executable(r7_range_split_plan_tests paper_r7_range_split_plan_tests.cpp)
 target_link_libraries(r7_range_split_plan_tests PRIVATE paper_r7)
 target_compile_options(r7_range_split_plan_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_range_split_plan_tests PRIVATE -Wl,--gc-sections)
endif()

option(ZKGPT_MASKED_TESTS "Build local Lasso masked research tests" OFF)
if(ZKGPT_MASKED_TESTS)
 add_executable(r7_masked_tests paper_r7_masked_tests.cpp)
 target_link_libraries(r7_masked_tests PRIVATE paper_r7)
 target_compile_options(r7_masked_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_masked_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_masked_input_guards COMMAND r7_masked_tests shape)
 add_test(NAME r7_masked_full_proofs COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/r7_masked_smoke.py $<TARGET_FILE:r7_masked_tests>)
endif()

if(ZKGPT_KIND01_TESTS)
 add_executable(r7_metadata_tests paper_r7_metadata_tests.cpp)
 target_link_libraries(r7_metadata_tests PRIVATE paper_r7)
 target_compile_options(r7_metadata_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_metadata_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_incremental_metadata COMMAND r7_metadata_tests)
endif()

option(ZKGPT_NATIVE_MASKED_TESTS "Build Native GKR masking research tests" OFF)
if(ZKGPT_NATIVE_MASKED_TESTS)
 add_executable(r7_native_masked_tests paper_r7_native_masked_tests.cpp)
 target_link_libraries(r7_native_masked_tests PRIVATE paper_r7)
 target_compile_options(r7_native_masked_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_native_masked_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_gkr_matrix_arithmetic COMMAND r7_native_masked_tests arithmetic)
 add_test(NAME r7_native_masked_full_proofs COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/r7_native_masked_smoke.py $<TARGET_FILE:r7_native_masked_tests>)
endif()

if(ZKGPT_NATIVE_MASKED_TESTS)
 add_executable(r7_fs_oneblock_prover paper_r7_oneblock_main.cpp)
 target_link_libraries(r7_fs_oneblock_prover PRIVATE paper_r7)
 target_compile_definitions(r7_fs_oneblock_prover PRIVATE R7_CLASSIFIED_V2=1 R7_FS_ONEBLOCK=1 R7_UNIFIED_INPUT1=1 R7_SINGLE_HYRAX=1)
 add_executable(r7_fs_oneblock_verifier paper_r7_oneblock_main.cpp)
 target_link_libraries(r7_fs_oneblock_verifier PRIVATE paper_r7)
 target_compile_definitions(r7_fs_oneblock_verifier PRIVATE R7_CLASSIFIED_V2=1 R7_FS_ONEBLOCK=1 R7_UNIFIED_INPUT1=1 R7_SINGLE_HYRAX=1 R7_COMPLETE_VERIFIER=1)
 target_compile_options(r7_fs_oneblock_prover PRIVATE -ffunction-sections -fdata-sections)
 target_compile_options(r7_fs_oneblock_verifier PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_fs_oneblock_prover PRIVATE -Wl,--gc-sections)
 target_link_options(r7_fs_oneblock_verifier PRIVATE -Wl,--gc-sections)
endif()

if(ZKGPT_NATIVE_MASKED_TESTS)
 add_executable(r7_combined_masked_tests paper_r7_combined_masked_tests.cpp)
 target_link_libraries(r7_combined_masked_tests PRIVATE paper_r7)
 target_compile_options(r7_combined_masked_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_combined_masked_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_combined_masked_full_proofs COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/r7_all_masked_smoke.py $<TARGET_FILE:r7_combined_masked_tests>)
endif()

option(ZKGPT_PUBLIC_K_TESTS "Build anonymous mmap PublicK tests" OFF)
if(ZKGPT_PUBLIC_K_TESTS)
 add_executable(r7_public_k_tests paper_r7_public_k_tests.cpp)
 target_link_libraries(r7_public_k_tests PRIVATE paper_r7)
 target_compile_options(r7_public_k_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_public_k_tests PRIVATE -Wl,--gc-sections -Wl,--wrap=mmap -Wl,--wrap=munmap)
 add_test(NAME r7_public_k_mmap COMMAND r7_public_k_tests)
endif()

option(ZKGPT_LAYOUT_VALIDATION_TESTS "Build R7 Packing and Unified Sumcheck tests" OFF)
if(ZKGPT_LAYOUT_VALIDATION_TESTS)
 add_executable(r7_layout_validation_tests paper_r7_layout_validation_tests.cpp)
 target_link_libraries(r7_layout_validation_tests PRIVATE paper_r7)
 target_compile_options(r7_layout_validation_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_layout_validation_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_unified_round_differential COMMAND r7_layout_validation_tests product)
 add_test(NAME r7_layout_validation_guards COMMAND r7_layout_validation_tests guards)
 add_test(NAME r7_layout_validation_protocols COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/r7_layout_validation_tests.py $<TARGET_FILE:r7_layout_validation_tests>)
endif()

option(ZKGPT_SINGLE_IDENTITY_TESTS "Build Single Hyrax identity and seal lifecycle tests" OFF)
if(ZKGPT_SINGLE_IDENTITY_TESTS)
 add_executable(r7_single_identity_tests paper_r7_single_identity_tests.cpp)
 target_link_libraries(r7_single_identity_tests PRIVATE paper_r7)
 target_compile_options(r7_single_identity_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_single_identity_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_single_identity_guards COMMAND r7_single_identity_tests guards)
 add_test(NAME r7_single_identity_protocols COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/r7_single_identity_tests.py $<TARGET_FILE:r7_single_identity_tests>)
endif()

option(ZKGPT_CATEGORY_FROZEN_TESTS "Build Category frozen mapping tests" OFF)
if(ZKGPT_CATEGORY_FROZEN_TESTS)
 add_executable(r7_category_frozen_tests paper_r7_category_frozen_tests.cpp)
 target_link_libraries(r7_category_frozen_tests PRIVATE paper_r7)
 target_compile_options(r7_category_frozen_tests PRIVATE -ffunction-sections -fdata-sections)
 target_link_options(r7_category_frozen_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_category_unbounded_index COMMAND r7_category_frozen_tests unbounded)
 add_test(NAME r7_category_frozen_guards COMMAND r7_category_frozen_tests guards)
 add_test(NAME r7_category_frozen_protocols COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/r7_category_frozen_tests.py $<TARGET_FILE:r7_category_frozen_tests>)
endif()

if(ZKGPT_CATEGORY_FROZEN_TESTS)
 add_executable(r7_single_root_tests paper_r7_single_root_tests.cpp)
 target_link_libraries(r7_single_root_tests PRIVATE paper_r7)
 target_compile_options(r7_single_root_tests PRIVATE -Wall -Wextra -Werror -ffunction-sections -fdata-sections)
 target_link_options(r7_single_root_tests PRIVATE -Wl,--gc-sections)
 add_test(NAME r7_single_root_layout COMMAND r7_single_root_tests layout)
 add_test(NAME r7_single_root_msm COMMAND r7_single_root_tests msm)
 add_test(NAME r7_single_root_protocol COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/r7_single_root_tests.py $<TARGET_FILE:r7_single_root_tests>)
endif()

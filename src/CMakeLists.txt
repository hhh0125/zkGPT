# aux_source_directory(. gpt_src)
# list(FILTER gpt_src EXCLUDE REGEX "main*")
# option(ENABLE_MY_ASSERT "Turn on MY_ASSERT checks" ON)
# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
# add_library(gpt_lib ${gpt_src})


# add_executable(demo_llm_run main_demo_llm.cpp)
# target_link_libraries(demo_llm_run gpt_lib mcl mclbn256)


aux_source_directory(. gpt_src)
list(FILTER gpt_src EXCLUDE REGEX "main*")
list(FILTER gpt_src EXCLUDE REGEX "(^|/)zk_(hiding_commitment|linear_opening|committed_linear_opening)\\.cpp$")

add_library(gpt_lib ${gpt_src})

# gpt_lib 的 include
target_include_directories(gpt_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CNPY_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rd/mcl/include
)


# gpt_lib 的 link
target_link_libraries(gpt_lib
    PRIVATE
    ${CNPY_LIBRARY}
    mcl
    mclbn256
    pthread
    z
)

add_executable(demo_llm_run main_demo_llm.cpp)
target_link_libraries(demo_llm_run PRIVATE gpt_lib)

add_library(gkr_test_support STATIC
    tests/gkr_test_fixture.cpp
    tests/zkgpt_artifact_test_fixture.cpp)
target_link_libraries(gkr_test_support PUBLIC gpt_lib)
target_include_directories(gkr_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(range_tests main_range_tests.cpp)
target_link_libraries(range_tests PRIVATE gkr_test_support)

add_executable(range_protocol_tests main_range_tests.cpp)
target_link_libraries(range_protocol_tests PRIVATE gkr_test_support)

add_executable(gkr_mutation_tests main_gkr_mutation_tests.cpp)
target_link_libraries(gkr_mutation_tests PRIVATE gkr_test_support)

add_executable(zkgpt_artifact_mutation_tests
    main_zkgpt_artifact_mutation_tests.cpp)
target_link_libraries(zkgpt_artifact_mutation_tests PRIVATE gkr_test_support)

add_executable(zkgpt_decoder_resource_tests
    main_zkgpt_decoder_resource_tests.cpp)
target_link_libraries(zkgpt_decoder_resource_tests PRIVATE gpt_lib)

add_test(NAME zkgpt_decoder_resource_tests
         COMMAND zkgpt_decoder_resource_tests)
set_tests_properties(zkgpt_decoder_resource_tests PROPERTIES TIMEOUT 30)

add_executable(range_artifact_verify main_range_artifact_verify.cpp)
target_link_libraries(range_artifact_verify PRIVATE gpt_lib)

add_executable(zkgpt_artifact_verify main_zkgpt_artifact_verify.cpp)
target_link_libraries(zkgpt_artifact_verify PRIVATE gpt_lib)

add_executable(lasso_folding_tests main_lasso_folding_tests.cpp)
target_link_libraries(lasso_folding_tests PRIVATE gpt_lib)
add_test(NAME lasso_folding_tests COMMAND lasso_folding_tests)
set_tests_properties(lasso_folding_tests PROPERTIES TIMEOUT 180)

add_executable(prover_lifecycle_tests main_prover_lifecycle_tests.cpp)
target_link_libraries(prover_lifecycle_tests PRIVATE gkr_test_support)
add_test(NAME prover_lifecycle_tests COMMAND prover_lifecycle_tests)

add_executable(hyrax_opening_tests main_hyrax_opening_tests.cpp)
target_link_libraries(hyrax_opening_tests PRIVATE gpt_lib)
add_test(NAME hyrax_opening_tests COMMAND hyrax_opening_tests)
set_tests_properties(hyrax_opening_tests PROPERTIES TIMEOUT 180)

add_executable(circuit_storage_tests main_circuit_storage_tests.cpp)
target_link_libraries(circuit_storage_tests PRIVATE gpt_lib)
add_test(NAME circuit_storage_tests COMMAND circuit_storage_tests)
set_tests_properties(circuit_storage_tests PROPERTIES TIMEOUT 180)

add_executable(model_binding main_model_binding.cpp)
target_link_libraries(model_binding PRIVATE gpt_lib)
add_executable(model_binding_tests main_model_binding_tests.cpp)
target_link_libraries(model_binding_tests PRIVATE gpt_lib)

add_executable(exp_lookup_tests main_exp_lookup_tests.cpp)
target_link_libraries(exp_lookup_tests PRIVATE gpt_lib)
add_test(NAME exp_lookup_tests COMMAND exp_lookup_tests)
set_tests_properties(exp_lookup_tests PROPERTIES TIMEOUT 240)

# Experimental primitive is opt-in and never linked into gpt_lib.
option(ZKGPT_BUILD_ZK_OPENING_PROTOTYPE "Build isolated experimental ZK primitive" OFF)
if(ZKGPT_BUILD_ZK_OPENING_PROTOTYPE)
    add_library(zk_opening_prototype STATIC
        zk_hiding_commitment.cpp zk_linear_opening.cpp)
    target_include_directories(zk_opening_prototype PUBLIC . ../3rd/mcl/include)
    target_link_libraries(zk_opening_prototype PUBLIC mcl mclbn256)
    target_compile_options(zk_opening_prototype PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(zk_opening_tests main_zk_opening_tests.cpp)
    target_link_libraries(zk_opening_tests PRIVATE zk_opening_prototype)
    target_compile_options(zk_opening_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME zk_opening_prototype_tests COMMAND zk_opening_tests)
    set_tests_properties(zk_opening_prototype_tests PROPERTIES TIMEOUT 30)
endif()

# Task11 experimental committed-y prototype; production gpt_lib excludes it.
option(ZKGPT_BUILD_COMMITTED_Y_PROTOTYPE "Build isolated committed-y ZK primitive" OFF)
if(ZKGPT_BUILD_COMMITTED_Y_PROTOTYPE)
    add_library(zk_committed_opening_prototype STATIC
        zk_hiding_commitment.cpp zk_linear_opening.cpp zk_committed_linear_opening.cpp)
    target_include_directories(zk_committed_opening_prototype PUBLIC . ../3rd/mcl/include)
    target_link_libraries(zk_committed_opening_prototype PUBLIC mcl mclbn256)
    target_compile_options(zk_committed_opening_prototype PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(zk_committed_opening_tests main_zk_committed_opening_tests.cpp)
    target_link_libraries(zk_committed_opening_tests PRIVATE zk_committed_opening_prototype)
    target_compile_options(zk_committed_opening_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(zk_committed_opening_verify main_zk_committed_opening_verify.cpp)
    target_link_libraries(zk_committed_opening_verify PRIVATE zk_committed_opening_prototype)
    target_compile_options(zk_committed_opening_verify PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME zk_committed_opening_tests COMMAND zk_committed_opening_tests)
    set_tests_properties(zk_committed_opening_tests PROPERTIES TIMEOUT 30)
endif()

# Route B Phase B1: isolated opt-in Lasso/Surge prototype.
option(ZKGPT_BUILD_LASSO_PROTOTYPE "Build isolated Lasso/Surge prototype" OFF)
if(ZKGPT_BUILD_LASSO_PROTOTYPE)
    add_subdirectory(lasso)
endif()

# Route B P0: isolated, default-off production source contract.
option(ZKGPT_ENABLE_ROUTE_B_PRODUCTION "Build Route B production source contract" OFF)
if(ZKGPT_ENABLE_ROUTE_B_PRODUCTION)
    add_subdirectory(route_b)
endif()

add_executable(r2_gkr_terminal_tests main_r2_gkr_terminal_tests.cpp)
target_link_libraries(r2_gkr_terminal_tests PRIVATE gkr_test_support)
add_test(NAME r2_gkr_terminals COMMAND r2_gkr_terminal_tests)

# R2 reuses existing production GKR and R1 lookup primitives.
add_subdirectory(paper_r1)
add_subdirectory(paper_r2)

# R7 framework is additive; R6 computation/proof targets remain unchanged.
option(ZKGPT_BUILD_R7_FRAMEWORK "Build fail-closed R7 Oneblock framework" OFF)
if(ZKGPT_BUILD_R7_FRAMEWORK)
    add_subdirectory(paper_r7)
endif()

add_executable(r5a_host_tests main_r5a_host_tests.cpp)
target_link_libraries(r5a_host_tests PRIVATE paper_r2)
foreach(mode counter round wide-gelu minimum missing codec arithmetic construct-boundaries wide-ln search-boundary input-config)
 add_test(NAME r5a_host_${mode} COMMAND r5a_host_tests ${mode})
endforeach()

add_executable(r5b_tests main_r5b_tests.cpp)
target_link_libraries(r5b_tests PRIVATE paper_r2)

foreach(mode round round-boundaries attention attention-boundaries ln ln-boundaries gelu gelu-boundaries arithmetic)
 add_test(NAME r5b_${mode} COMMAND r5b_tests ${mode})
endforeach()

add_test(NAME r5b_public_layout COMMAND r5b_tests public-layout)

add_executable(r5b_gkr_tests main_r5b_gkr_tests.cpp)
target_link_libraries(r5b_gkr_tests PRIVATE paper_r2)
add_test(NAME r5b_FC_GKR_coupling COMMAND r5b_gkr_tests)

add_executable(opt2_ownership_tests main_opt2_ownership_tests.cpp)
target_link_libraries(opt2_ownership_tests PRIVATE gpt_lib gkr_test_support)
add_test(NAME opt2_shared_ownership COMMAND opt2_ownership_tests)

add_executable(opt2_allocation_tests main_opt2_allocation_tests.cpp)
target_link_libraries(opt2_allocation_tests PRIVATE gpt_lib gkr_test_support)
add_test(NAME opt2_master_allocation COMMAND opt2_allocation_tests)

add_test(NAME opt2_postpublication_diagnostic_failure COMMAND opt2_allocation_tests --postpublish)

# aux_source_directory(. gpt_src)
# list(FILTER gpt_src EXCLUDE REGEX "main*")
# option(ENABLE_MY_ASSERT "Turn on MY_ASSERT checks" ON)
# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
# add_library(gpt_lib ${gpt_src})


# add_executable(demo_llm_run main_demo_llm.cpp)
# target_link_libraries(demo_llm_run gpt_lib mcl mclbn256)


aux_source_directory(. gpt_src)
list(FILTER gpt_src EXCLUDE REGEX "main*")

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

add_executable(range_tests main_range_tests.cpp)
target_link_libraries(range_tests PRIVATE gpt_lib)

add_executable(range_protocol_tests main_range_tests.cpp)
target_link_libraries(range_protocol_tests PRIVATE gpt_lib)

add_executable(range_artifact_verify main_range_artifact_verify.cpp)
target_link_libraries(range_artifact_verify PRIVATE gpt_lib)

add_executable(zkgpt_artifact_verify main_zkgpt_artifact_verify.cpp)
target_link_libraries(zkgpt_artifact_verify PRIVATE gpt_lib)

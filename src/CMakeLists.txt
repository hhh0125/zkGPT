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

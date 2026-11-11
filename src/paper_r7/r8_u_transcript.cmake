# A bounded, deterministic unified-reduction regression. Full opening/P/V
# coverage is recorded separately by the real oneblock/full12 experiment.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(stage "${R8_OUTPUT}/${suffix}")
file(MAKE_DIRECTORY "${stage}")
foreach(mode deferred immediate auto)
 foreach(role prove verify)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
   ZKGPT_THREADS=4 ZKGPT_U_THREADS=4 ZKGPT_CATEGORY_U_FOLD=1
   ZKGPT_CATEGORY_U_RAM_BYTES=81920 ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES=16777216
   "ZKGPT_CATEGORY_U_FOLD_MODE=${mode}"
   "${R8_EXECUTABLE}" "${role}" "${stage}/${mode}.fsproof"
   RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  file(WRITE "${stage}/${mode}-${role}.log" "${output}\n${error}")
  if(NOT status EQUAL 0)
   message(FATAL_ERROR "R8 ${mode} ${role} failed; evidence: ${stage}")
  endif()
 endforeach()
 file(SHA256 "${stage}/${mode}.fsproof" digest)
 if(mode STREQUAL "deferred")
  set(reference "${digest}")
 elseif(NOT digest STREQUAL reference)
  message(FATAL_ERROR "R8 fixed-random proof/transcript mismatch: ${stage}")
 endif()
endforeach()
message(STATUS "R8 deterministic proof bytes and independent bounded verification PASS: ${reference}")

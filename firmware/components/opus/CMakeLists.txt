# libopus as an ESP-IDF component (fixed-point build for ESP32-S3).
# Source is fetched by ./fetch-upstream.sh into ./upstream (git-ignored).
set(OPUS_DIR "${CMAKE_CURRENT_LIST_DIR}/upstream")
if(NOT EXISTS "${OPUS_DIR}/opus_sources.mk")
    message(FATAL_ERROR "libopus source missing: run firmware/components/opus/fetch-upstream.sh")
endif()

# Parse a "VAR = a \\\n b \\\n c" list out of the upstream *.mk files.
function(opus_sources_from_mk out_var var_name mk_file)
    file(READ "${mk_file}" content)
    string(REGEX REPLACE "\\\\\r?\n" " " content "${content}")
    string(REGEX MATCH "(^|\n)${var_name} = ([^\n]*)" _m "${content}")
    string(STRIP "${CMAKE_MATCH_2}" line)
    string(REGEX REPLACE " +" ";" files "${line}")
    set(result "")
    foreach(f ${files})
        list(APPEND result "${OPUS_DIR}/${f}")
    endforeach()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

opus_sources_from_mk(CELT_SRCS       CELT_SOURCES       "${OPUS_DIR}/celt_sources.mk")
opus_sources_from_mk(SILK_SRCS       SILK_SOURCES       "${OPUS_DIR}/silk_sources.mk")
opus_sources_from_mk(SILK_FIXED_SRCS SILK_SOURCES_FIXED "${OPUS_DIR}/silk_sources.mk")
opus_sources_from_mk(OPUS_SRCS       OPUS_SOURCES       "${OPUS_DIR}/opus_sources.mk")

idf_component_register(
    SRCS ${CELT_SRCS} ${SILK_SRCS} ${SILK_FIXED_SRCS} ${OPUS_SRCS}
    INCLUDE_DIRS "${OPUS_DIR}/include"
    PRIV_INCLUDE_DIRS "${OPUS_DIR}" "${OPUS_DIR}/celt" "${OPUS_DIR}/silk"
                      "${OPUS_DIR}/silk/fixed" "${OPUS_DIR}/src"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OPUS_BUILD
    FIXED_POINT=1
    DISABLE_FLOAT_API
    USE_ALLOCA
    HAVE_LRINT
    HAVE_LRINTF
    PACKAGE_VERSION="1.5.2"
)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2 -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
    -Wno-maybe-uninitialized -Wno-implicit-fallthrough -Wno-stringop-overflow
)

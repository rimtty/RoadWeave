# A regression guard for the test harness itself: different images must fail,
# missing references must fail, and neither path may rewrite the reference.
configure_file("${BASELINE}" reference.png COPYONLY)
file(SHA256 reference.png before)
file(REMOVE diff.png actual.png missing.png reference_err.png)
execute_process(COMMAND "${EXE}" --scenario tx --capture actual.png --reference reference.png
    RESULT_VARIABLE mismatch)
if(NOT mismatch EQUAL 1 OR NOT EXISTS diff.png OR NOT EXISTS reference_err.png)
    message(FATAL_ERROR "A changed frame must fail and produce difference images.")
endif()
file(SHA256 reference.png after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "Comparison changed the reference image.")
endif()
execute_process(COMMAND "${EXE}" --scenario group --capture actual.png --reference missing.png
    RESULT_VARIABLE missing)
if(NOT missing EQUAL 1 OR EXISTS missing.png)
    message(FATAL_ERROR "Missing references must fail without creating a new baseline.")
endif()

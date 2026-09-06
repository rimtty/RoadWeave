# Copy approved references into results: LVGL writes mismatch images beside them.
if(NOT EXISTS "${BASELINE}")
    message(FATAL_ERROR "Missing approved baseline: ${BASELINE}. Capture and review it first.")
endif()
configure_file("${BASELINE}" reference.png COPYONLY)
file(REMOVE actual.png reference_err.png diff.png)
execute_process(COMMAND "${EXE}" --scenario "${SCENARIO}" --capture actual.png
    --reference reference.png RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${SCENARIO} failed (${result}); inspect actual.png, reference.png and diff.png here.")
endif()

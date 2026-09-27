function(apply_or_check patch_path)
    execute_process(
        COMMAND git apply --ignore-whitespace "${patch_path}"
        RESULT_VARIABLE apply_result
        ERROR_QUIET
    )
    if(NOT apply_result EQUAL 0)
        execute_process(
            COMMAND git apply --ignore-whitespace --reverse --check "${patch_path}"
            RESULT_VARIABLE reverse_result
            ERROR_QUIET
        )
        if(NOT reverse_result EQUAL 0)
            message(FATAL_ERROR "Failed to apply or verify patch ${patch_path}")
        endif()
    endif()
endfunction()

apply_or_check("${patch_file}")
if(DEFINED patch_file_2)
    apply_or_check("${patch_file_2}")
endif()

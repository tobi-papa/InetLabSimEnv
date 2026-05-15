option(ENTROPY_ENABLE_ASAN  "Enable AddressSanitizer + UBSan" OFF)
option(ENTROPY_ENABLE_TSAN  "Enable ThreadSanitizer"          OFF)

function(apply_sanitizers target)
    if(ENTROPY_ENABLE_ASAN AND ENTROPY_ENABLE_TSAN)
        message(FATAL_ERROR "ENTROPY_ENABLE_ASAN and ENTROPY_ENABLE_TSAN cannot be enabled simultaneously.")
    endif()
    if(ENTROPY_ENABLE_ASAN)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined)
    endif()
    if(ENTROPY_ENABLE_TSAN)
        target_compile_options(${target} PRIVATE -fsanitize=thread)
        target_link_options(${target} PRIVATE   -fsanitize=thread)
    endif()
endfunction()

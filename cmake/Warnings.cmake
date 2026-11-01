# Warnings-as-errors for our own targets only (never applied to fetched code).
function(mm_set_warnings target)
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
                               -Wno-unused-parameter)
    endif()
endfunction()

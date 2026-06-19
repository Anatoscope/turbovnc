
if(UNIX AND NOT APPLE)
    # Defined a new build type based on Release mode
    # but with debug information
    set(new_mode "AppWithDebInfo")
    string(TOUPPER ${new_mode} upper_mode)

    set(extra_flag "-g")
    set(extra_linker_flag "")

    set( CMAKE_CXX_FLAGS_${upper_mode} "${CMAKE_CXX_FLAGS_RELEASE} ${extra_flag}" CACHE STRING
        "Flags used by the C++ compiler during ${new_mode} builds."
        FORCE )
    set( CMAKE_C_FLAGS_${upper_mode} "${CMAKE_C_FLAGS_RELEASE} ${extra_flag}" CACHE STRING
        "Flags used by the C compiler during ${new_mode} builds."
        FORCE )
    set( CMAKE_EXE_LINKER_FLAGS_${upper_mode}
        "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${extra_linker_flag}" CACHE STRING
        "Flags used for linking binaries during ${new_mode} builds."
        FORCE )
    set( CMAKE_SHARED_LINKER_FLAGS_${upper_mode}
        "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${extra_linker_flag}" CACHE STRING
        "Flags used by the shared libraries linker during ${new_mode} builds."
        FORCE )
    set(CMAKE_STATIC_LINKER_FLAGS_${upper_mode}
        "${CMAKE_STATIC_LINKER_FLAGS} ${extra_linker_flag}" CACHE STRING
        "Flags used by the static libraries linker during ${new_mode} builds."
        FORCE )

    # Hide new variables
    mark_as_advanced(
        CMAKE_CXX_FLAGS_${upper_mode}
        CMAKE_C_FLAGS_${upper_mode}
        CMAKE_EXE_LINKER_FLAGS_${upper_mode}
        CMAKE_SHARED_LINKER_FLAGS_${upper_mode}
        CMAKE_STATIC_LINKER_FLAGS_${upper_mode})

    # Update the documentation string of CMAKE_BUILD_TYPE for GUIs
    get_property(currentHelpString CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING)
    string(FIND "${currentHelpString}" "${new_mode}" alreadythere)
    if( ${alreadythere} EQUAL -1)
        set( CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "${currentHelpString} ${new_mode}" FORCE )
    endif()

endif()

# Strip debug info on Linux to a .debug file.
# Only work on AppWithDebInfo mode.
# Come with install rules
function(strip_debug_info target_name)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "AppWithDebInfo")

        set(input_file $<TARGET_FILE:${target_name}>)

        set(output_debug_file ${input_file}.debug)

        find_program(OBJCOPY objcopy )
        if (OBJCOPY STREQUAL "OBJCOPY-NOTFOUND")
            message(FATAL_ERROR "objcopy not found")
        endif()

        add_custom_command(
            TARGET ${target_name}
            POST_BUILD
            VERBATIM
            COMMAND ${OBJCOPY} --only-keep-debug ${input_file} ${output_debug_file}
            COMMAND ${OBJCOPY} --strip-unneeded ${input_file}
            COMMAND ${OBJCOPY} --add-gnu-debuglink=${output_debug_file} ${input_file}
            COMMENT "Stripping symbols from ${input_file} into file ${output_debug_file}."
            )
    endif()
endfunction()

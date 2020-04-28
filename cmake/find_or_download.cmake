function(find_or_download PACKAGE)
    set(options EXACT INSTALL_WITH_HiCMA)
    set(oneValueArgs VERSION)
    cmake_parse_arguments(ARGS
        "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
    )
    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unparsed arguments: ${ARGS_UNPARSED_ARGUMENTS}.\n"
            "Ensure that correct arguments are passed to find_or_download!"
        )
    endif()
    if(${ARGS_EXACT})
        set(EXACT "EXACT")
    endif()
    find_package(${PACKAGE} ${ARGS_VERSION} ${EXACT} QUIET)
    if(${${PACKAGE}_FOUND})
        message(STATUS "Found dependency ${PACKAGE} installed in system.")
    else()
        message(STATUS "Suitable version of ${PACKAGE} not found in system.")
        message(STATUS "Downloading ${PACKAGE} and building from source.")

        if(${ARGS_INSTALL_WITH_HiCMA})
            set(DEPENDENCY_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
        else()
            set(DEPENDENCY_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/dependencies)
        endif()
        # Use below settings for git downloads if available
        if(${CMAKE_VERSION} VERSION_GREATER 3.6)
            list(APPEND ADDITIONAL_GIT_SETTINGS "GIT_SHALLOW True")
        endif()
        if(${CMAKE_VERSION} VERSION_GREATER 3.8)
            list(APPEND ADDITIONAL_GIT_SETTINGS
                "GIT_PROGRESS True GIT_CONFIG advice.detachedHead=false"
            )
        endif()
        # Prepare download instructions for dependency
        configure_file(
            ${CMAKE_SOURCE_DIR}/cmake/${PACKAGE}_download.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}_download/CMakeLists.txt
            @ONLY
        )

        # Configure step for download instructions
        execute_process(
            COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}_download
            OUTPUT_QUIET
        )
        if(result)
            message(FATAL_ERROR
                "Download of dependency ${PACKAGE} failed: ${result}."
            )
        endif()

        # Download, build and install dependency according to instructions
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build .
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}_download
        )
        if(result)
            message(FATAL_ERROR
                "Build of dependency ${PACKAGE} failed: ${result}."
            )
        endif()

        # Update search path and use regular find_package to add dependency
        find_package(${PACKAGE}
            ${ARGS_VERSION} ${EXACT} REQUIRED NO_DEFAULT_PATH
            PATHS "${DEPENDENCY_INSTALL_PREFIX}"
        )
        message(STATUS "Using ${PACKAGE} from ${DEPENDENCY_INSTALL_PREFIX}.")
    endif()
endfunction()

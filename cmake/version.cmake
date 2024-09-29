#
# See public gist at: https://gist.github.com/CrankOne/b110c3cfe1e9943da6278982b5fdf6a1
#
# Gets package version variables based on Git tag or sets version variable's
# based on `version.txt' file found in the source dir (for platforms without
# git). Tags are expected to be given in forms:
#
#       v<major:int>.<minor:int>
#       v<major:int>.<minor:int>.<patch:int>
#
# Usage:
#
#   include (cmake/version.cmake)
#   get_pkg_version (foo)
#
# Will result in following variables defined:
#
#   foo_VERSION_MAJOR   # <major:int> from tag
#   foo_VERSION_MINOR   # <major:int> from tag
#   foo_VERSION_PATCH   # <patch:int> from tag or 0
#   foo_VERSION_TWEAK   # orderly commit number wrt last version tag
#   foo_REF             # branch name with `/' -> `.'
#
# User code then can utilize this information on their own convenience, like:
#
#   project (foo VERSION ${foo_VERSION_MAJOR}.${foo_VERSION_MINOR})

macro(parse_version_strexpr STREXPR_ PKG_PREFIX)
    # Handles `git describe' result
    if( ${STREXPR_} MATCHES "^v([0-9]+)\\.([0-9]+)-([0-9]+)-g([a-f0-9]+)$" )
        # Case of major, minor, and commit version
        # example: "v1.0-2-g6c92bf7"
        set(${PKG_PREFIX}_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(${PKG_PREFIX}_VERSION_MINOR ${CMAKE_MATCH_2})
        set(${PKG_PREFIX}_VERSION_PATCH "")
        set(${PKG_PREFIX}_VERSION_TWEAK ${CMAKE_MATCH_3})
    elseif(${STREXPR_} MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)-([0-9]+)-g([a-f0-9]+)$")
        # Case of major, minor, tweak, and commit version
        # example: "v1.2.3-4-g6c92bf7"
        set(${PKG_PREFIX}_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(${PKG_PREFIX}_VERSION_MINOR ${CMAKE_MATCH_2})
        set(${PKG_PREFIX}_VERSION_PATCH ${CMAKE_MATCH_3})
        set(${PKG_PREFIX}_VERSION_TWEAK ${CMAKE_MATCH_4})
    elseif(${STREXPR_} MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        # Case with major, minor and patch only
        # example: "v1.2.3"
        set(${PKG_PREFIX}_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(${PKG_PREFIX}_VERSION_MINOR ${CMAKE_MATCH_2})
        set(${PKG_PREFIX}_VERSION_PATCH ${CMAKE_MATCH_3})
        set(${PKG_PREFIX}_VERSION_TWEAK 0)
    elseif(${STREXPR_} MATCHES "^v([0-9]+)\\.([0-9]+)$")
        # Case with major and minor only
        # example: "v1.2"
        set(${PKG_PREFIX}_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(${PKG_PREFIX}_VERSION_MINOR ${CMAKE_MATCH_2})
        set(${PKG_PREFIX}_VERSION_PATCH "")
        set(${PKG_PREFIX}_VERSION_TWEAK 0)
    else()
        message(FATAL_ERROR "Failed to get or interpret version number from"
            " \"${STREXPR_}\"")
    endif()
    message(DEBUG "${PKG_PREFIX} version: \"${STREXPR_}\" -> major=${${PKG_PREFIX}_VERSION_MAJOR}"
        " minor=${${PKG_PREFIX}_VERSION_MINOR} patch=${${PKG_PREFIX}_VERSION_PATCH}"
        " tweak=${${PKG_PREFIX}_VERSION_TWEAK}")
endmacro()

macro(get_pkg_version PKG_PREFIX)
    find_package(Git)
    if (GIT_FOUND)
        message(STATUS "Git found: ${GIT_EXECUTABLE} in version ${GIT_VERSION_STRING}")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --match "v[0-9]*.[0-9]*" --tags
            RESULT_VARIABLE GIT_TAG_RESULT
            OUTPUT_VARIABLE GIT_TAG_OUTPUT
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
        string(STRIP ${GIT_TAG_OUTPUT} GIT_TAG_OUTPUT)
        parse_version_strexpr(${GIT_TAG_OUTPUT} ${PKG_PREFIX})
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            RESULT_VARIABLE GIT_REF_RESULT
            OUTPUT_VARIABLE GIT_REF_OUTPUT
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
        string(STRIP ${GIT_REF_OUTPUT} _${PKG_PREFIX}_REF)
        # substitute slashes (`/') in branch name with `.'
        string(REPLACE "/" "." ${PKG_PREFIX}_REF ${_${PKG_PREFIX}_REF})
    elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/version.txt)
        message(STATUS "Found version file: ${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" VER_STR_EXPR)
        string(STRIP ${VER_STR_EXPR} VER_STR_EXPR)
        parse_version_strexpr(${VER_STR_EXPR} ${PKG_PREFIX})
        set(${PKG_PREFIX}_REF "")
    endif (GIT_FOUND)
    message(STATUS "${PKG_PREFIX} version: major=${${PKG_PREFIX}_VERSION_MAJOR}"
        " minor=${${PKG_PREFIX}_VERSION_MINOR} patch=${${PKG_PREFIX}_VERSION_PATCH}"
        " tweak=${${PKG_PREFIX}_VERSION_TWEAK} ref=\"${${PKG_PREFIX}_REF}\"")
    file (WRITE
          "${CMAKE_CURRENT_BINARY_DIR}/version.txt"
          "${${PKG_PREFIX}_VERSION_MAJOR}.${${PKG_PREFIX}_VERSION_MINOR}"
         )
     if(NOT "${${PKG_PREFIX}_VERSION_PATCH}" STREQUAL "")
        file (APPEND
          "${CMAKE_CURRENT_BINARY_DIR}/version.txt"
          ".${${PKG_PREFIX}_VERSION_PATCH}"
         )
    endif()
    if(${${PKG_PREFIX}_VERSION_TWEAK} GREATER 0)
        file (APPEND
          "${CMAKE_CURRENT_BINARY_DIR}/version.txt"
          ".${${PKG_PREFIX}_VERSION_TWEAK}"
         )
    endif()
    set(_DFT_REFS "master" "main" "trunk" "HEAD")
    if(NOT ${${PKG_PREFIX}_REF} IN_LIST _DFT_REFS)
        file (APPEND
              "${CMAKE_CURRENT_BINARY_DIR}/version.txt"
              ".${${PKG_PREFIX}_REF}"
             )
    endif()
    file (APPEND
              "${CMAKE_CURRENT_BINARY_DIR}/version.txt"
              "\n"
            )
endmacro(get_pkg_version)


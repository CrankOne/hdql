cmake_minimum_required (VERSION 3.9.4)

# HDQL CMake config file
#  - provides debug and release builds
#  - has extra targets for (un-)install and code coverage
#  - exports install configuration
#  - packaging via CPack

set (hdql_VERSION_MAJOR 0)
set (hdql_VERSION_MINOR 1)
set (hdql_VERSION ${hdql_VERSION_MAJOR}.${hdql_VERSION_MINOR})

project (hdql VERSION ${hdql_VERSION})

option (BUILD_TESTS "Enable unit tests (affects object code being built)" OFF)
option (TYPES_DEBUG "Enables some crude type checks" OFF)
option (COVERAGE "Enables special compiler flags for coverage tests" OFF)
# Forcing -fPIC option is useful when compiling static library that supposed to
# be further linked in a share library. Still, users might want to disable this
# option for their static libs, so we provide here option to suppress it
option (FORCE_FPIC "Enables -fPIC for compiled code" ON)

set (hdql_LIB hdql )
set (hdql_SOURCES src/types.cc
	              src/errors.c
	              src/attr-def.c
	              src/compound.cc
	              src/query.cc
	              src/query-key.c
	              src/query-product.c
	              src/context.cc
	              src/operations.cc
	              src/value.cc
	              src/function.cc
                  src/converters.cc
	              src/ifaces/fwd-query-as-collection.c
	              src/ifaces/fwd-query-as-scalar.c
	              src/ifaces/arith-op-as-scalar.c
	              src/ifaces/arith-op-as-collection.c
	              src/ifaces/filtered-v-compound.c
                  src/funcs/generic-logic.c
                  src/helpers/query.cc
                  )

add_compile_options(-Wall)

#
# Look for GTest if required
if (BUILD_TESTS)
    find_package (GTest REQUIRED)
    # Workaround for CMake bug found in some versions in 2012-2104, see:
    #   - https://stackoverflow.com/a/29871891/1734499
    set (THREADS_PREFER_PTHREAD_FLAG ON)
    find_package (Threads REQUIRED)
    if (COVERAGE)
        set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
        include (CodeCoverage)
        set (COVERAGE_EXCLUDES "test/*")
        append_coverage_compiler_flags ()
        setup_target_for_coverage_gcovr_html (NAME hdql-discover-coverage
                                        EXECUTABLE hdql-discover
                                        BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
                                        EXCLUDE "obj/*;build/*;doc/*;templates/*")
    endif (COVERAGE)
endif (BUILD_TESTS)

#
# Generate parser
find_package (BISON)
find_package (FLEX)

bison_target (HDQL_PARSER hdql.y
              ${CMAKE_CURRENT_BINARY_DIR}/hdql.tab.c
              COMPILE_FLAGS "-d -p hdql_ -Wall"  # dev note: use -Wcounterexamples for newer versions
              DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/hdql.tab.h)
flex_target (HDQL_LEXER hdql.l
             ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
             DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.h)
add_flex_bison_dependency (HDQL_LEXER HDQL_PARSER)

list (APPEND hdql_SOURCES
      ${BISON_HDQL_PARSER_OUTPUTS}
      ${FLEX_HDQL_LEXER_OUTPUTS})

#
# Library target

set (CMAKE_DEBUG_POSTFIX .dbg)

add_library (hdql ${hdql_SOURCES})
target_include_directories (hdql
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
    )

set_target_properties (hdql PROPERTIES
            VERSION ${PROJECT_VERSION}
          SOVERSION ${PROJECT_VERSION_MAJOR}
            )
set_target_properties (hdql PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

if (TYPES_DEBUG)
    target_compile_definitions (hdql PUBLIC HDQL_TYPES_DEBUG=1)
endif (TYPES_DEBUG)

if (BUILD_TESTS)
    target_link_libraries (hdql PUBLIC ${GTEST_BOTH_LIBRARIES} Threads::Threads)
    target_compile_definitions (hdql PUBLIC BUILD_GT_UTEST=1)
    set (hdqlTest_SOURCES test/main.cc
                          test/events-struct.cc
                          test/samples.cc
                          test/iteration-tests.cc
                          test/cpp-api.cc
                          )
    add_executable (hdql-test ${hdqlTest_SOURCES})
    target_link_libraries (hdql-test PUBLIC hdql)
    set_target_properties (hdql-test PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
    if (COVERAGE)
        setup_target_for_coverage_gcovr_html (NAME hdql-test-coverage
                                              EXECUTABLE hdql-test
                                              BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
                                              EXCLUDE "obj/*;build/*;doc/*;templates/*")
    endif (COVERAGE)
endif (BUILD_TESTS)

#
# (Un)install
target_sources (hdql PUBLIC FILE_SET HEADERS
    BASE_DIRS include
    FILES include/hdql/attr-def.h
          include/hdql/compound.h
          include/hdql/context.h
          include/hdql/errors.h
          include/hdql/function.h
          include/hdql/internal-ifaces.h
          include/hdql/operations.h
          include/hdql/query-key.h
          include/hdql/query-product.h
          include/hdql/query.h
          include/hdql/types.h
          include/hdql/value.h
          include/hdql/helpers/compounds.hh
          include/hdql/helpers/functions.hh
          include/hdql/helpers/query.hh
          )
include (GNUInstallDirs)
install (TARGETS hdql
    EXPORT ${CMAKE_PROJECT_NAME}Targets
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET HEADERS
    )
       
#
# Uninstall target (caveat: leaves empty dirs and broken symlinks)
add_custom_target (uninstall
        "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/uninstall.cmake"
    )

#
# CMake config export
install (EXPORT ${CMAKE_PROJECT_NAME}Targets
    FILE ${CMAKE_PROJECT_NAME}Targets.cmake
    DESTINATION lib/cmake/${CMAKE_PROJECT_NAME}
    )
include (CMakePackageConfigHelpers)
configure_package_config_file (${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "lib/cmake/${CMAKE_PROJECT_NAME}"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )
write_basic_package_version_file (
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${hdql_VERSION}"
    COMPATIBILITY AnyNewerVersion
    )
install (FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
    DESTINATION lib/cmake/${CMAKE_PROJECT_NAME}
    )
# Permits using hdql from build dir (without installing)
export(EXPORT ${CMAKE_PROJECT_NAME}Targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Targets.cmake"
    )

# CPack aux (packaging)
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${hdql_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${hdql_VERSION_MINOR}")
set(CPACK_SOURCE_GENERATOR "TBZ2")
include(CPack)


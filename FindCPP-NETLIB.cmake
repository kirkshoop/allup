# Finds the C++ Network (CPP-NETLIB) Library
#
#  CPP-NETLIB_INCLUDE_DIRS   - Directory to include to get CPP-NETLIB headers
#  CPP-NETLIB_LIBRARIES      - Libraries to link against for the common CPP-NETLIB

SET(_CPP-NETLIB_ALL_PLUGINS    uri concurrency message message-wrappers message-directives constants http-client-connections http-client message-wrappers message http-message-wrappers http-message)
SET(_CPP-NETLIB_REQUIRED_VARS  CPP-NETLIB_INCLUDE_DIR CPP-NETLIB_LIBRARY CPP-NETLIB_REQUIRED_LIBRARY )

option( CPP-NETLIB_DISABLE_LOGGING "Disable logging definitely, no logging code will be generated or compiled." OFF )

if( NOT CPP-NETLIB_DISABLE_LOGGING )
  set( _CPP-NETLIB_ALL_PLUGINS ${_CPP-NETLIB_ALL_PLUGINS} logging )
  set( CPP-NETLIB_FIND_COMPONENTS ${CPP-NETLIB_FIND_COMPONENTS} logging )
endif()

#
### FIRST STEP: Find the headers.
#
FIND_PATH(
    CPP-NETLIB_INCLUDE_DIR http/src/network/http/client.hpp
    PATH "${CPP-NETLIB_SOURCE_DIR}"
    DOC "cppnetlib include directory")
MARK_AS_ADVANCED(CPP-NETLIB_INCLUDE_DIR)

SET(CPP-NETLIB_INCLUDE_DIRS 
    ${CPP-NETLIB_INCLUDE_DIR}/uri/src
    ${CPP-NETLIB_INCLUDE_DIR}/concurrency/src
    ${CPP-NETLIB_INCLUDE_DIR}/message/src
    ${CPP-NETLIB_INCLUDE_DIR}/logging/src
    ${CPP-NETLIB_INCLUDE_DIR}/http/src
    ${CPP-NETLIB_INCLUDE_DIR})

#
### THIRD STEP: Find all installed plugins if the header was found
#
IF(CPP-NETLIB_INCLUDE_DIR)

    MESSAGE(STATUS "Looking for libraries")
    FOREACH(plugin IN LISTS _CPP-NETLIB_ALL_PLUGINS)

        FIND_LIBRARY(
            CPP-NETLIB_${plugin}_PLUGIN
            NAMES cppnetlib-${plugin}
            HINTS ${CPP-NETLIB_TARGET_DIR}
            PATH_SUFFIXES "" "http/src" "uri/src" "concurrency/src" "message/src" "logging/src" "lib${LIB_SUFFIX}")
        MARK_AS_ADVANCED(CPP-NETLIB_${plugin}_PLUGIN)

        IF(CPP-NETLIB_${plugin}_PLUGIN)
            MESSAGE(STATUS "    * Plugin ${plugin} found ${CPP-NETLIB_${plugin}_LIBRARY}.")
            SET(CPP-NETLIB_${plugin}_FOUND True)
            SET(CPP-NETLIB_LIBRARY ${CPP-NETLIB_LIBRARY} ${CPP-NETLIB_${plugin}_PLUGIN})
        ELSE()
            MESSAGE(STATUS "    * Plugin ${plugin} not found.")
            SET(CPP-NETLIB_${plugin}_FOUND False)
        ENDIF()

    ENDFOREACH()

    #
    ### FOURTH CHECK: Check if the required components were all found
    #
    FOREACH(component ${CPP-NETLIB_FIND_COMPONENTS})
        IF(${CPP-NETLIB_${component}_FOUND})
            # Does not work with NOT ... . No idea why.
        ELSE()
            MESSAGE(SEND_ERROR "Required component ${component} not found.")
        ENDIF()
    ENDFOREACH()

ENDIF()

FOREACH(component ${CPP-NETLIB_FIND_COMPONENTS})
    SET(CPP-NETLIB_REQUIRED_LIBRARY ${CPP-NETLIB_REQUIRED_LIBRARY} ${CPP-NETLIB_${component}_PLUGIN})
ENDFOREACH()

#
### ADHERE TO STANDARDS
#
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CPP-NETLIB DEFAULT_MSG ${_CPP-NETLIB_REQUIRED_VARS})


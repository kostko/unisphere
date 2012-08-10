#---------------------------------------------------------------------------------------------------
#
#  Copyright (C) 2010  Artem Rodygin
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#---------------------------------------------------------------------------------------------------
#
#  This module finds if Botan library is installed and determines where required
#  include files and libraries are. The module sets the following variables:
#
#    Botan_FOUND         - system has Botan
#    Botan_INCLUDE_DIR   - the Botan include directory
#    Botan_LIBRARIES     - the libraries needed to use Botan
#    Botan_VERSION       - Botan full version information string
#    Botan_VERSION_MAJOR - the major version of the Botan release
#    Botan_VERSION_MINOR - the minor version of the Botan release
#    Botan_VERSION_PATCH - the patch version of the Botan release
#
#  You can help the module to find Botan by specifying its root path in
#  environment variable named "BOTAN_ROOT". If this variable is not set
#  then module will search for files in default path as following:
#
#    CMAKE_HOST_WIN32 - "C:\Program Files\Botan"
#    CMAKE_HOST_UNIX  - "/usr", "/usr/local"
#
#---------------------------------------------------------------------------------------------------

set(Botan_FOUND TRUE)

file(TO_CMAKE_PATH "$ENV{BOTAN_ROOT}" BOTAN_ROOT)

# search for headers

if (WIN32)

    find_path(Botan_INCLUDE_DIR
              NAMES "botan/build.h"
              PATHS "C:/Program Files/Botan"
                    "C:/Program Files (x86)/Botan"
              ENV BOTAN_ROOT
              PATH_SUFFIXES "include")

else (WIN32)

    find_path(Botan_INCLUDE_DIR
              NAMES "botan/build.h"
              PATHS "/usr"
                    "/usr/local"
              ENV BOTAN_ROOT
              PATH_SUFFIXES "include")

endif (WIN32)

# headers are found

if (Botan_INCLUDE_DIR)

    # retrieve version information from the header

    file(READ "${Botan_INCLUDE_DIR}/botan/build.h" BUILD_H_FILE)

    string(REGEX REPLACE ".*#define[ \t]+BOTAN_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" Botan_VERSION_MAJOR "${BUILD_H_FILE}")
    string(REGEX REPLACE ".*#define[ \t]+BOTAN_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" Botan_VERSION_MINOR "${BUILD_H_FILE}")
    string(REGEX REPLACE ".*#define[ \t]+BOTAN_VERSION_PATCH[ \t]+([0-9]+).*" "\\1" Botan_VERSION_PATCH "${BUILD_H_FILE}")

    set(Botan_VERSION "${Botan_VERSION_MAJOR}.${Botan_VERSION_MINOR}.${Botan_VERSION_PATCH}")

    # search for library

    if (WIN32)

        file(GLOB Botan_LIBRARIES
             "C:/Program Files/Botan/botan.lib"
             "C:/Program Files (x86)/Botan/botan.lib"
             "${BOTAN_ROOT}/botan.lib")

    else (WIN32)

        file(GLOB Botan_LIBRARIES
             "/usr/lib/libbotan-${Botan_VERSION_MAJOR}.${Botan_VERSION_MINOR}.*.so"
             "/usr/local/lib/libbotan-${Botan_VERSION_MAJOR}.${Botan_VERSION_MINOR}.*.so"
             "${BOTAN_ROOT}/lib/libbotan-${Botan_VERSION_MAJOR}.${Botan_VERSION_MINOR}.*.so")

    endif (WIN32)

endif (Botan_INCLUDE_DIR)

# headers are not found

if (NOT Botan_INCLUDE_DIR)
    set(Botan_FOUND FALSE)
endif (NOT Botan_INCLUDE_DIR)

# library is not found

if (NOT Botan_LIBRARIES)
    set(Botan_FOUND FALSE)
endif (NOT Botan_LIBRARIES)

# set default error message

if (Botan_FIND_VERSION)
    set(Botan_ERROR_MESSAGE "Unable to find Botan library v${Botan_FIND_VERSION}")
else (Botan_FIND_VERSION)
    set(Botan_ERROR_MESSAGE "Unable to find Botan library")
endif (Botan_FIND_VERSION)

# check found version

if (Botan_FIND_VERSION AND Botan_FOUND)

    if (Botan_FIND_VERSION_EXACT)
        if (NOT ${Botan_VERSION} VERSION_EQUAL ${Botan_FIND_VERSION})
            set(Botan_FOUND FALSE)
        endif (NOT ${Botan_VERSION} VERSION_EQUAL ${Botan_FIND_VERSION})
    else (Botan_FIND_VERSION_EXACT)
        if (${Botan_VERSION} VERSION_LESS ${Botan_FIND_VERSION})
            set(Botan_FOUND FALSE)
        endif (${Botan_VERSION} VERSION_LESS ${Botan_FIND_VERSION})
    endif (Botan_FIND_VERSION_EXACT)

    if (NOT Botan_FOUND)
        set(Botan_ERROR_MESSAGE "Unable to find Botan library v${Botan_FIND_VERSION} (${Botan_VERSION} was found)")
    endif (NOT Botan_FOUND)

endif (Botan_FIND_VERSION AND Botan_FOUND)

# final status messages

if (Botan_FOUND)

    if (NOT Botan_FIND_QUIETLY)
        message(STATUS "Botan version: ${Botan_VERSION}")
    endif (NOT Botan_FIND_QUIETLY)

    mark_as_advanced(Botan_INCLUDE_DIR
                     Botan_LIBRARIES)

else (Botan_FOUND)

    if (Botan_FIND_REQUIRED)
        message(SEND_ERROR "${Botan_ERROR_MESSAGE}")
    endif (Botan_FIND_REQUIRED)

endif (Botan_FOUND)

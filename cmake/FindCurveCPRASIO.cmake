
# - Find libcurvecpr-asio
# Find the native libcurvecpr-asio bindings.
# Once done this will define
#
#  CURVECPRASIO_INCLUDE_DIR    - where to find libsodium header files, etc.
#  CURVECPRASIO_FOUND          - True if libsodium found.
#

FIND_PATH(CURVECPRASIO_INCLUDE_DIR curvecp/curvecp.hpp)

# handle the QUIETLY and REQUIRED arguments and set CURVECPRASIO_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CurveCPRASIO REQUIRED_VARS CURVECPRASIO_INCLUDE_DIR)

MARK_AS_ADVANCED(CURVECPRASIO_INCLUDE_DIR)

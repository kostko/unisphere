
# - Find MongoDB C++ bindings
# Once done this will define
#
#  MONGODB_INCLUDE_DIR    - where to find libmongoclient header files, etc.
#  MONGODB_LIBRARY        - List of libraries when using libmongoclient.
#  MONGODB_FOUND          - True if libmongoclient found.
#

FIND_LIBRARY(MONGODB_LIBRARY NAMES mongoclient libmongoclient HINTS ${MONGODB_LIB_DIR})
FIND_PATH(MONGODB_INCLUDE_DIR mongo/version.h)

# handle the QUIETLY and REQUIRED arguments and set MONGODB_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MongoClient REQUIRED_VARS MONGODB_LIBRARY MONGODB_INCLUDE_DIR)

MARK_AS_ADVANCED(MONGODB_LIBRARY MONGODB_INCLUDE_DIR)

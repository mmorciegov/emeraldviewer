# -*- cmake -*-

# - Find ELFIO
# Find the ELFIO includes and library
# This module defines
#  ELFIO_INCLUDE_DIR, where to find elfio.h, etc.
#  ELFIO_LIBRARIES, the libraries needed to use ELFIO.
#  ELFIO_FOUND, If false, do not try to use ELFIO.
# also defined, but not for general use are
#  ELFIO_LIBRARY, where to find the ELFIO library.

FIND_PATH(ELFIO_INCLUDE_DIR ELFIO/ELFIO.h
/usr/local/include
/usr/include
)

SET(ELFIO_NAMES ${ELFIO_NAMES} ELFIO)
FIND_LIBRARY(ELFIO_LIBRARY
  NAMES ${ELFIO_NAMES}
  PATHS /usr/lib /usr/local/lib
  )

IF (ELFIO_LIBRARY AND ELFIO_INCLUDE_DIR)
    SET(ELFIO_LIBRARIES ${ELFIO_LIBRARY})
    SET(ELFIO_FOUND "YES")
ELSE (ELFIO_LIBRARY AND ELFIO_INCLUDE_DIR)
  SET(ELFIO_FOUND "NO")
ENDIF (ELFIO_LIBRARY AND ELFIO_INCLUDE_DIR)


IF (ELFIO_FOUND)
   IF (NOT ELFIO_FIND_QUIETLY)
      MESSAGE(STATUS "Found ELFIO: ${ELFIO_LIBRARIES}")
   ENDIF (NOT ELFIO_FIND_QUIETLY)
ELSE (ELFIO_FOUND)
   IF (ELFIO_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find ELFIO library")
   ENDIF (ELFIO_FIND_REQUIRED)
ENDIF (ELFIO_FOUND)

# Deprecated declarations.
SET (NATIVE_ELFIO_INCLUDE_PATH ${ELFIO_INCLUDE_DIR} )
GET_FILENAME_COMPONENT (NATIVE_ELFIO_LIB_PATH ${ELFIO_LIBRARY} PATH)

MARK_AS_ADVANCED(
  ELFIO_LIBRARY
  ELFIO_INCLUDE_DIR
  )

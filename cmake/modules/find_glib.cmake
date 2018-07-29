

include(FindPkgConfig)
include(FindPackageHandleStandardArgs)
pkg_search_module(GLIB_PKGCONF glibmm-2.4)

find_library(GLIB_LIBRARY
    NAME glibmm-2.4
    PATHS ${GLIB_PKGCONF_LIBRARY_DIRS})

if(GLIB_PKGCONF_FOUND)
    set(GLIB_INCLUDE_DIRS ${GLIB_PKGCONF_INCLUDE_DIRS})
    set(GLIB_LIBRARIES ${GLIB_LIBRARY})
    set(GLIB_FOUND yes)
else()
    set(GLIB_LIBRARIES)
    set(GLIB_INCLUDE_DIRS)
    set(GLIB_FOUND no)
endif()

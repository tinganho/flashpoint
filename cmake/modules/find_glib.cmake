

include(FindPkgConfig)
include(FindPackageHandleStandardArgs)
pkg_check_modules(GLIB_PKGCONF REQUIRED glibmm-2.4 glib-2.0)

find_library(GLIB_LIBRARY
    NAME glibmm-2.0.a
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

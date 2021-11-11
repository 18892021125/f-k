# FindMVE
# --------
#
# Find MVE includes and libraries.  Once done this will define
#
#   MVE_INCLUDE_DIRS    - where to find headers
#   MVE_DMRECON_LIBRARY - where to find MVE's dmrecon library
#   MVE_MVE_LIBRARY     - where to find MVE's mve library
#   MVE_OGL_LIBRARY     - where to find MVE's ogl library
#   MVE_SFM_LIBRARY     - where to find MVE's sfm library
#   MVE_UTIL_LIBRARY    - where to find MVE's util library
#   MVE_FOUND           - True if MVE is found.
#
# An includer may set MVE_ROOT to a mve installation root to tell this
# module where to look.

set(MVE_INCLUDE_DIR ${MVE_ROOT}/libs CACHE PATH "MVE include dir")
find_library(MVE_DMRECON_LIBRARY_RELEASE NAMES mve_dmrecon
	HINTS ${MVE_ROOT}/libs/dmrecon
		  ${MVE_ROOT}/libs/dmrecon/Release
		  ${MVE_ROOT}/build/libs/dmrecon/Release)
find_library(MVE_DMRECON_LIBRARY_DEBUG NAMES mve_dmrecon_d
	HINTS ${MVE_ROOT}/libs/dmrecon
		  ${MVE_ROOT}/libs/dmrecon/Debug
		  ${MVE_ROOT}/build/libs/dmrecon/Debug)

find_library(MVE_MVE_LIBRARY_RELEASE NAMES mve
	HINTS ${MVE_ROOT}/libs/mve
		  ${MVE_ROOT}/libs/mve/Release
		  ${MVE_ROOT}/build/libs/mve/Release)
	  find_library(MVE_MVE_LIBRARY_DEBUG NAMES mve_d
	HINTS ${MVE_ROOT}/libs/mve
		  ${MVE_ROOT}/libs/mve/Debug
		  ${MVE_ROOT}/build/libs/mve/Debug)

find_library(MVE_OGL_LIBRARY_RELEASE NAMES mve_ogl
	HINTS ${MVE_ROOT}/libs/ogl
		  ${MVE_ROOT}/libs/ogl/Release
		  ${MVE_ROOT}/build/libs/ogl/Release)
find_library(MVE_OGL_LIBRARY_DEBUG NAMES mve_ogl_d
	HINTS ${MVE_ROOT}/libs/ogl
		  ${MVE_ROOT}/libs/ogl/Debug
		  ${MVE_ROOT}/build/libs/ogl/Debug)

find_library(MVE_SFM_LIBRARY_RELEASE NAMES mve_sfm
	HINTS ${MVE_ROOT}/libs/sfm
		  ${MVE_ROOT}/libs/sfm/Release
		  ${MVE_ROOT}/build/libs/sfm/Release)
	  find_library(MVE_SFM_LIBRARY_DEBUG NAMES mve_sfm_d
	HINTS ${MVE_ROOT}/libs/sfm
		  ${MVE_ROOT}/libs/sfm/Debug
		  ${MVE_ROOT}/build/libs/sfm/Debug)

find_library(MVE_UTIL_LIBRARY_RELEASE NAMES mve_util
	HINTS ${MVE_ROOT}/libs/util
		  ${MVE_ROOT}/libs/util/Release
		  ${MVE_ROOT}/build/libs/util/Release)
find_library(MVE_UTIL_LIBRARY_DEBUG NAMES mve_util_d
	HINTS ${MVE_ROOT}/libs/util
		  ${MVE_ROOT}/libs/util/Debug
		  ${MVE_ROOT}/build/libs/util/Debug)

if(MVE_DMRECON_LIBRARY_RELEASE)
	set(MVE_DMRECON_LIBRARY optimized ${MVE_DMRECON_LIBRARY_RELEASE})
endif()
if(MVE_DMRECON_LIBRARY_DEBUG)
	set(MVE_DMRECON_LIBRARY ${MVE_DMRECON_LIBRARY} debug ${MVE_DMRECON_LIBRARY_DEBUG})
endif()

if(MVE_MVE_LIBRARY_RELEASE)
	set(MVE_MVE_LIBRARY optimized ${MVE_MVE_LIBRARY_RELEASE})
endif()
if(MVE_MVE_LIBRARY_DEBUG)
	set(MVE_MVE_LIBRARY ${MVE_MVE_LIBRARY} debug ${MVE_MVE_LIBRARY_DEBUG})
endif()

if(MVE_OGL_LIBRARY_RELEASE)
	set(MVE_OGL_LIBRARY optimized ${MVE_OGL_LIBRARY_RELEASE})
endif()
if(MVE_OGL_LIBRARY_DEBUG)
	set(MVE_OGL_LIBRARY ${MVE_OGL_LIBRARY} debug ${MVE_OGL_LIBRARY_DEBUG})
endif()

if(MVE_SFM_LIBRARY_RELEASE)
	set(MVE_SFM_LIBRARY optimized ${MVE_SFM_LIBRARY_RELEASE})
endif()
if(MVE_SFM_LIBRARY_DEBUG)
	set(MVE_SFM_LIBRARY ${MVE_SFM_LIBRARY} debug ${MVE_SFM_LIBRARY_DEBUG})
endif()

if(MVE_UTIL_LIBRARY_RELEASE)
	set(MVE_UTIL_LIBRARY optimized ${MVE_UTIL_LIBRARY_RELEASE})
endif()
if(MVE_UTIL_LIBRARY_DEBUG)
	set(MVE_UTIL_LIBRARY ${MVE_UTIL_LIBRARY} debug ${MVE_UTIL_LIBRARY_DEBUG})
endif()

mark_as_advanced(
	MVE_DMRECON_LIBRARY
	MVE_MVE_LIBRARY
	MVE_OGL_LIBRARY
	MVE_SFM_LIBRARY
	MVE_UTIL_LIBRARY
	MVE_INCLUDE_DIR)

if(MVE_DMRECON_LIBRARY)
	message("Found MVE_DMRECON: ${MVE_DMRECON_LIBRARY}")
endif()

if(MVE_MVE_LIBRARY)
	message("Found MVE: ${MVE_MVE_LIBRARY}")
endif()

if(MVE_OGL_LIBRARY)
	message("Found MVE_OGL: ${MVE_OGL_LIBRARY}")
endif()

if(MVE_SFM_LIBRARY)
	message("Found MVE_SFM: ${MVE_SFM_LIBRARY}")
endif()

if(MVE_UTIL_LIBRARY)
	message("Found MVE_UTIL: ${MVE_UTIL_LIBRARY}")
endif()

# handle the QUIETLY and REQUIRED arguments and set MVE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	MVE REQUIRED_VARS MVE_DMRECON_LIBRARY MVE_MVE_LIBRARY MVE_OGL_LIBRARY MVE_SFM_LIBRARY MVE_UTIL_LIBRARY MVE_INCLUDE_DIR)

if(MVE_FOUND)
	set(MVE_INCLUDE_DIRS ${MVE_INCLUDE_DIR})
endif()


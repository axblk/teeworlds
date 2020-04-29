set_extra_dirs_lib(WGPU wgpu)
find_library(WGPU_LIBRARY
  NAMES wgpu_native.dll wgpu_native
  HINTS ${HINTS_WGPU_LIBDIR}
  PATHS ${PATHS_WGPU_LIBDIR}
)
set_extra_dirs_include(WGPU wgpu "${WGPU_LIBRARY}")
find_path(WGPU_INCLUDEDIR
  NAMES wgpu.h
  HINTS ${HINTS_WGPU_INCLUDEDIR}
  PATHS ${PATHS_WGPU_INCLUDEDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WGPU DEFAULT_MSG WGPU_LIBRARY WGPU_INCLUDEDIR)

mark_as_advanced(WGPU_LIBRARY WGPU_INCLUDEDIR)

if(WGPU_FOUND)
  set(WGPU_LIBRARIES ${WGPU_LIBRARY})
  set(WGPU_INCLUDE_DIRS ${WGPU_INCLUDEDIR})

  is_bundled(WGPU_BUNDLED "${WGPU_LIBRARY}")
  if(WGPU_BUNDLED AND TARGET_OS STREQUAL "windows")
    set(WGPU_COPY_FILES "${EXTRA_WGPU_LIBDIR}/wgpu_native.dll")
  else()
    set(WGPU_COPY_FILES)
  endif()
endif()

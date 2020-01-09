if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(SQLite3)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})

  if(NOT SQLite3_FOUND)
    find_path(SQLite3_INCLUDEDIR NAMES sqlite3.h)
    find_library(SQLite3_LIBRARY NAMES sqlite3 sqlite)

    mark_as_advanced(SQLite3_LIBRARY SQLite3_INCLUDEDIR)

    if(SQLite3_INCLUDEDIR)
      file(STRINGS ${SQLite3_INCLUDEDIR}/sqlite3.h _ver_line
          REGEX "^#define SQLITE_VERSION  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
          LIMIT_COUNT 1)
      string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
            SQLite3_VERSION "${_ver_line}")
      unset(_ver_line)
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(SQLite3
        REQUIRED_VARS SQLite3_INCLUDEDIR SQLite3_LIBRARY
        VERSION_VAR SQLite3_VERSION)
    
    if(SQLite3_FOUND)
      set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDEDIR})
      set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
    endif()
  endif()

  if(SQLite3_FOUND)
    set(SQLite3_BUNDLED OFF)
    set(SQLite3_DEP)
  endif()
endif()

if(NOT SQLite3_FOUND)
  set(SQLite3_BUNDLED ON)
  set(SQLite3_SRC_DIR src/engine/external/sqlite)
  set_src(SQLite3_SRC GLOB ${SQLite3_SRC_DIR}
    sqlite3.c
    sqlite3.h
  )
  add_library(sqlite3 EXCLUDE_FROM_ALL OBJECT ${SQLite3_SRC})
  set(SQLite3_INCLUDEDIR ${SQLite3_SRC_DIR})
  target_include_directories(sqlite3 PRIVATE ${SQLite3_INCLUDEDIR})

  set(SQLite3_DEP $<TARGET_OBJECTS:sqlite3>)
  set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDEDIR})
  set(SQLite3_LIBRARIES ${CMAKE_DL_LIBS})

  list(APPEND TARGETS_DEP sqlite3)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(SQLite3 DEFAULT_MSG SQLite3_INCLUDEDIR)
endif()

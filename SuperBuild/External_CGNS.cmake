
# The CGNS external project for ParaView
set(cgns_source "${CMAKE_CURRENT_BINARY_DIR}/cgns")
set(cgns_install "${CMAKE_CURRENT_BINARY_DIR}/cgns-install")

# If Windows we use CMake otherwise ./configure
if(WIN32)
  
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(cgns_64 -64)
  else()
    set(cgns_64)
  endif()
  
  # we need the short path to zlib and hdf5
  execute_process(
    COMMAND cscript /NoLogo ${CMAKE_CURRENT_SOURCE_DIR}/shortpath.vbs ${zlib_install}
    OUTPUT_VARIABLE zlib_dos_short_path
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(
    COMMAND cscript /NoLogo ${CMAKE_CURRENT_SOURCE_DIR}/shortpath.vbs ${hdf5_install}
    OUTPUT_VARIABLE hdf5_dos_short_path
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # getting short path doesnt work on directories that dont exist
  file(MAKE_DIRECTORY ${cgns_install})
  execute_process(
    COMMAND cscript /NoLogo ${CMAKE_CURRENT_SOURCE_DIR}/shortpath.vbs ${cgns_install}
    OUTPUT_VARIABLE cgns_install_dos_short_path
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  ExternalProject_Add(CGNS
    URL ${CGNS_URL}/${CGNS_GZ}
    URL_MD5 ${CGNS_MD5}
    SOURCE_DIR ${cgns_source}
    BUILD_IN_SOURCE 1
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ParaViewSuperBuild_CMAKE_SOURCE_DIR}/CGNSPatches/cgnslib.h" "${cgns_source}/cgnslib.h"
    CONFIGURE_COMMAND configure -install ${cgns_install_dos_short_path} -dll ${cgns_64} -zlib ${zlib_dos_short_path}
    BUILD_COMMAND nmake
    INSTALL_COMMAND nmake install
    DEPENDS ${CGNS_dependencies}
    )

elseif(APPLE)
  # cgns only appears to build statically on mac.

  # cgns install system sucks..
  file(MAKE_DIRECTORY ${cgns_install}/lib)
  file(MAKE_DIRECTORY ${cgns_install}/include)

  ExternalProject_Add(CGNS
    SOURCE_DIR ${cgns_source}
    INSTALL_DIR ${cgns_install}
    URL ${CGNS_URL}/${CGNS_GZ}
    URL_MD5 ${CGNS_MD5}
    BUILD_IN_SOURCE 1
    PATCH_COMMAND ""
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-64bit --without-fortran
    DEPENDS ${CGNS_dependencies}
  )

else()

  # cgns install system sucks..
  file(MAKE_DIRECTORY ${cgns_install}/lib)
  file(MAKE_DIRECTORY ${cgns_install}/include)

  ExternalProject_Add(CGNS
    SOURCE_DIR ${cgns_source}
    INSTALL_DIR ${cgns_install}
    URL ${CGNS_URL}/${CGNS_GZ}
    URL_MD5 ${CGNS_MD5}
    BUILD_IN_SOURCE 1
    PATCH_COMMAND ""
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-zlib=${ZLIB_LIBRARY} --with-hdf5=${hdf5_install} --enable-64bit --enable-shared=all --disable-static --without-fortran
    DEPENDS ${CGNS_dependencies}
  )

endif()

set(CGNS_INCLUDE_DIR ${cgns_install}/include)

if(APPLE)
  set(CGNS_LIBRARY ${cgns_install}/lib/libcgns.a)
else()
  set(CGNS_LIBRARY ${cgns_install}/lib/libcgns${_LINK_LIBRARY_SUFFIX})
endif()

FIND_PACKAGE(MPI)
INCLUDE_DIRECTORIES(
  ${MPI_INCLUDE_PATH}
  ${PYTHON_INCLUDE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/compat"
  "${CMAKE_CURRENT_BINARY_DIR}/compat"
  "${CMAKE_CURRENT_SOURCE_DIR}/config"
  "${CMAKE_CURRENT_BINARY_DIR}/config"
  )
PYTHON_ADD_MODULE(MPIPython MODULE MPI.c)
SET_PROPERTY(TARGET MPIPython PROPERTY PREFIX "")
SET_PROPERTY(TARGET MPIPython PROPERTY LINKER_LANGUAGE CXX)
TARGET_LINK_LIBRARIES(MPIPython ${MPI_LIBRARY} ${PYTHON_LIBRARY})
IF (MPI_EXTRA_LIBRARY)
  TARGET_LINK_LIBRARIES(MPIPython ${MPI_EXTRA_LIBRARY})
ENDIF (MPI_EXTRA_LIBRARY)


INSTALL(TARGETS MPIPython
  RUNTIME DESTINATION ${PV_INSTALL_BIN_DIR} COMPONENT Runtime
  LIBRARY DESTINATION ${PV_INSTALL_LIB_DIR} COMPONENT Runtime
  ARCHIVE DESTINATION ${PV_INSTALL_LIB_DIR} COMPONENT Development)

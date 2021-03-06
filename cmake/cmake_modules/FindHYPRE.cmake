find_path (HYPRE_DIR include/HYPRE.h HINTS "${HYPRE_HINTS}")
if( EXISTS ${HYPRE_DIR}/include/HYPRE.h )
  set(HYPRE_FOUND YES)
  set(HYPRE_INCLUDES ${HYPRE_DIR})
  find_path(HYPRE_INCLUDE_DIR HYPRE.h HYPRE_parcsr_ls.h HINTS "${HYPRE_DIR}" PATH_SUFFIXES include NO_DEFAULT_PATH)
  list(APPEND HYPRE_INCLUDES ${HYPRE_INCLUDE_DIR})
  find_library(HYPRE_LIBRARIES HYPRE PATHS ${HYPRE_DIR}/lib)
endif( EXISTS ${HYPRE_DIR}/include/HYPRE.h )
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HYPRE DEFAULT_MSG HYPRE_LIBRARIES HYPRE_INCLUDES)

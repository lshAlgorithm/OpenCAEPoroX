#

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: Debug, Release, RelWithDebInfo, or MinSizeRel." FORCE)
endif()

option(OCP_USE_MPI "Enable MPI parallel build" ON)
option(OCP_USE_OPENMP "Enable OpenMP parallel build" OFF)
option(OCP_USE_LAPACK "Enable LAPACK usage" ON)
option(OCP_USE_METIS "Enable METIS usage" OFF)
option(OCP_USE_PARMETIS "Enable ParMETIS usage" OFF)
option(OCP_USE_PETSC "Enable PETSc usage" OFF)
option(OCP_USE_PETSCSOLVER "Enable PETSC_SOLVER usage" OFF)
option(OCP_USE_FASP "Enable FASP usage" OFF)
option(OCP_USE_FASP4BLKOIL "Enable PETSC_SOLVER usage" OFF)

option(OCP_ENABLE_TESTING "Enable the ctest framework for testing" OFF)


set(MPI_ROOT "" CACHE PATH "Path to the MPI library.")

set(OPENMP "" CACHE PATH "Path to the OpenMPI library.")

set(BLAS_INCLUDE_DIRS "" CACHE STRING "Path to BLAS headers.")
set(BLAS_LIBRARIES "" CACHE STRING "The BLAS library.")
set(LAPACK_INCLUDE_DIRS "" CACHE STRING "Path to LAPACK headers.")
set(LAPACK_LIBRARIES "" CACHE STRING "The LAPACK library.")

set(METIS_DIR "" CACHE PATH "Path to the METIS library.")

set(PARMETIS_DIR "" CACHE PATH "Path to the ParMETIS library.")

set(PETSC_DIR "" CACHE PATH "Path to the PETSc library.")

set(PETSCSOLVER_DIR "" CACHE PATH "Path to the petsc-solver library.")

set(FASP_DIR "" CACHE PATH "Path to the FASP library.")

set(FASP4BLKOIL_DIR "" CACHE PATH "Path to the FASP4BlackOil library.")

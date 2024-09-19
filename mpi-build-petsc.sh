#!/bin/bash


# source /es01/paratera/parasoft/module.sh  
# source /es01/paratera/parasoft/oneAPI/2022.1/setvars.sh
# module load cmake/3.17.1 gcc/7.3.0-para 

export CC=mpiicx
export CXX=mpiicpx

# users specific directory paths
ROOT_DIR=/home/brianlee/Documents/asc/OpenCAEPoro
export PARMETIS_DIR=$ROOT_DIR/parmetis-4.0.3
export PARMETIS_BUILD_DIR=$ROOT_DIR/parmetis-4.0.3/build/Linux-x86_64
export METIS_DIR=$ROOT_DIR/parmetis-4.0.3/metis
export METIS_BUILD_DIR=$ROOT_DIR/parmetis-4.0.3/build/Linux-x86_64
export PETSC_DIR=$ROOT_DIR/petsc-3.19.3
export PETSC_ARCH=petsc_install
export PETSCSOLVER_DIR=$ROOT_DIR/petsc_solver
# export HYPRE_DIR=$ROOT_DIR/hypre-2.28.0/hypre-install

export CPATH=$ROOT_DIR/petsc-3.19.3/include/:$CPATH
export CPATH=$ROOT_DIR/petsc-3.19.3/petsc_install/include/:$ROOT_DIR/parmetis-4.0.3/metis/include:$ROOT_DIR/parmetis-4.0.3/include:$CPATH
export CPATH=$ROOT_DIR/lapack-3.11/CBLAS/include/:$CPATH
export CPATH=$ROOT_DIR/petsc-3.19.3/src/ksp/pc/impls/hypre/:$CPATH


# install
rm -fr build; mkdir build; cd build;

echo "cmake -DUSE_OPENMP=ON -DUSE_PETSCSOLVER=ON -DUSE_PARMETIS=ON -DUSE_METIS=ON -DCMAKE_VERBOSE_MAKEFILE=OFF -DCMAKE_BUILD_TYPE=Debug .."
cmake -DUSE_OPENMP=ON -DUSE_PETSCSOLVER=ON -DUSE_PARMETIS=ON -DUSE_METIS=ON -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_BUILD_TYPE=Debug ..
#cmake -DUSE_PARMETIS=ON -DUSE_METIS=ON -DUSE_PETSCSOLVER=ON -DCMAKE_VERBOSE_MAKEFILE=OFF -DCMAKE_BUILD_TYPE=Release ..

echo "make -j 32"
make -j 32 #LDFLAGS="${LDFLAGS} -L$HYPRE_DIR/lib -lHYPRE" CPPFLAGS="${CPPFLAGS} -I$HYPRE_DIR/bin"

echo "make install"
make install

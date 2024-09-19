#!/bin/sh
#SBATCH -p G1Part_sce
#SBATCH -N 1
#SBATCH -n 56
#SBATCH --exclusive

# source /es01/paratera/parasoft/module.sh  
# source /es01/paratera/parasoft/oneAPI/2022.1/setvars.sh
# module load cmake/3.17.1 gcc/7.3.0-para
# unset I_MPI_PMI_LIBRARY
# export I_MPI_JOB_RESPECT_PROCESS_PLACEMENT=0


export OMP_NUM_THREADS=3    # 指定线程数
for np    # 指定进程数
in 4
do
	mpirun -n $np ./testOpenCAEPoro ./data/test/test.data verbose=1
done

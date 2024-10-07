/*! \file    PetscSolver.cpp
 *  \brief   API of Petsc Solver
 *  \author  Shizhe Li
 *  \date    May/04/2023
 *
 *  \note    use SAMG as linear solver
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifdef WITH_PETSCSOLVER

#include "PetscSolver.hpp"

double MATRIX_CONVERSION_TIME;

 /// Allocate memoery for pardiso solver
void PetscSolver::Allocate(const OCP_USI& max_nnz, const OCP_USI& maxDim)
{
    A.resize(max_nnz * blockdim * blockdim);
    iA.resize(maxDim + 1);
    jA.resize(max_nnz);
}


/// Calculate terms used in communication
void PetscSolver::CalCommTerm(const USI& actWellNum, const Domain* domain)
{
    global_index  = domain->CalGlobalIndex(actWellNum);
    const OCP_INT numElementloc = actWellNum + domain->GetNumGridInterior();

    GetWallTime timer;
    timer.Start();

    MPI_Allgather(&numElementloc, 1, MPI_INT, &allEle[0], 1, OCPMPI_INT, myComm);

    OCPTIME_COMM_COLLECTIVE += timer.Stop() / TIME_S2MS;
    
    allBegin[0] = 0;
    allEnd[0]   = allEle[0] - 1;
    for (OCP_USI p = 1; p < numproc; p++) {
        allBegin[p] = allEnd[p - 1] + 1;
        allEnd[p]   = allBegin[p] + allEle[p] - 1;
    }
}


/// Assemble coefficient matrix.
void PetscSolver::AssembleMat(const vector<vector<USI>>& colId,
    const vector<vector<OCP_DBL>>& val,
    const OCP_USI& dim,
    vector<OCP_DBL>& rhs,
    vector<OCP_DBL>& u)
{

    const USI blockSize = blockdim * blockdim;
    vector<OCP_SLL> tmpJ;
    // Assemble iA, jA, A

    #pragma omp single
    {
    iA[0] = 0;

    // #pragma omp for schedule(guided)
    for (OCP_USI i = 1; i < dim + 1; i++) {
        const USI nnzR = colId[i - 1].size();

        tmpJ.resize(nnzR);
        
        for (USI j = 0; j < nnzR; j++) {
            tmpJ[j] = global_index->at(colId[i - 1][j]);
        }

        iA[i] = iA[i - 1] + nnzR;
        copy(tmpJ.begin(), tmpJ.end(), &jA[iA[i - 1]]);
        copy(val[i - 1].begin(), val[i - 1].end(), &A[(iA[i - 1]) * blockSize]);
    }


    b = rhs.data();
    x = u.data();
    }
}


OCP_INT ScalarPetscSolver::Solve()
{
    OCP_ABORT("Inavailable!");
}


OCP_INT VectorPetscSolver::Solve()
{
    // printf("#DEBUG: %s %s call FIM_solver_p\n", __FILE__, __FUNCTION__);
    int precondID = 0;  // -1: CPR, 0: MSP, 1: BAMG
    int is_thermal = 0; // 0: iso-thermal, 1: thermal

    // old_petsc: msp
    // return FIM_solver_p(myrank, numproc, blockdim, allBegin.data(), allEnd.data(), iA.data(), jA.data(), A.data(), b, x);
    
    // new_petsc: msp、cpr、bamg
    // return FIM_solver_p(precondID, is_thermal, myrank, numproc, blockdim, allBegin.data(), allEnd.data(), iA.data(), jA.data(), A.data(), b, x, &MATRIX_CONVERSION_TIME);
    /*
        Where is the implementation of FIM_solver? -- Li Shuhuai
    */
    return FIM_solver_p(precondID, is_thermal, myrank, numproc, blockdim, allBegin.data(), allEnd.data(), iA.data(), jA.data(), A.data(), b, x, &MATRIX_CONVERSION_TIME);
    // if(myrank == 0) printf("MATRIX_CONVERSION_TIME = %f s\n", MATRIX_CONVERSION_TIME);
}

#endif // WITH_PETSCSOLVER



 /*----------------------------------------------------------------------------*/
 /*  Brief Change History of This File                                         */
 /*----------------------------------------------------------------------------*/
 /*  Author              Date             Actions                              */
 /*----------------------------------------------------------------------------*/
 /*  Shizhe Li           May/04/2023      Create file                          */
 /*----------------------------------------------------------------------------*/
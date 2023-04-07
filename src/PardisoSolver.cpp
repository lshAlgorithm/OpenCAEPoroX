/*! \file    PardisoSolver.cpp
 *  \brief   PardisoSolver for OpenCAEPoro simulator
 *  \author  Shizhe Li
 *  \date    Feb/21/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoro team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "PardisoSolver.hpp"


void PardisoSolver::SetupParam(const string& dir, const string& file)
{
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    InitParam();
}


void PardisoSolver::InitParam()
{
    iparm[0]  = 1;  /* Solver default parameters overriden with provided by iparm */
    iparm[1]  = 2;  /* Use METIS for fill-in reordering */
    iparm[5]  = 0;  /* Write solution into x */
    iparm[7]  = 2;  /* Max number of iterative refinement steps */
    iparm[9]  = 13; /* Perturb the pivot elements with 1E-13 */
    iparm[10] = 1;  /* Use nonsymmetric permutation and scaling MPS */
    iparm[12] = 1;  /* Switch on Maximum Weighted Matching algorithm (default for non-symmetric) */
    iparm[17] = 0;  /* (closed) Output: Number of nonzeros in the factor LU */ 
    iparm[18] = 0;  /* (closed) Output: Mflops for LU factorization */
    iparm[26] = 1;  /* Check input data for correctness */
    iparm[39] = 2;  /* Input: matrix/rhs/solution are distributed between MPI processes  */

    /*Zero-based indexing: columns and rows indexing in arrays ia, ja, and perm starts from 0 (C-style indexing).*/
    iparm[34] = 1;  
}


void PardisoSolver::Allocate(const OCP_USI& max_nnz, const OCP_USI& maxDim, const USI& blockDim)
{
    blockdim = blockDim;
    if (blockdim > 1) {
        iparm[37] = blockdim * blockdim;
    }

    iA.resize(maxDim + 1);
    jA.resize(max_nnz);
    A.resize(max_nnz * blockdim * blockdim);
}


/// Calculate terms used in communication
void PardisoSolver::CalCommTerm(const USI& actWellNum, const Domain* domain)
{
    global_index = domain->CalGlobalIndex(actWellNum);

    const OCP_USI numGridInterior = domain->GetNumGridInterior();
    const OCP_USI numElementLoc   = actWellNum + numGridInterior;
    const OCP_USI global_end      = global_index->at(numElementLoc - 1);

    iparm[40] = global_end - numElementLoc + 1;  // global begin (included)
    iparm[41] = global_end;                      // global end   (included)

    // Get Dimension
    N = global_end + 1;
    MPI_Bcast(&N, 1, MPI_INT, domain->numproc - 1, domain->myComm);
}


/// Assemble coefficient matrix.
void PardisoSolver::AssembleMat(const vector<vector<USI>>& colId,
    const vector<vector<OCP_DBL>>& val,
    const OCP_USI& dim,
    vector<OCP_DBL>& rhs,
    vector<OCP_DBL>& u)
{
    const USI blockSize = blockdim * blockdim;
    vector<USI> tmp;  
    // Assemble iA, jA, A
    iA[0] = 0;
    for (OCP_USI i = 1; i < dim + 1; i++) {
        const USI nnzR = colId[i - 1].size();

        iA[i] = iA[i - 1] + nnzR;
        // reorder
        tmp = colId[i - 1];
        for (auto& t : tmp)    t = global_index->at(t);
        sort(tmp.begin(), tmp.end());
        for (USI j0 = 0; j0 < nnzR; j0++) {
            jA[iA[i - 1] + j0] = tmp[j0];
            for (USI j1 = 0; j1 < nnzR; j1++) {
                if (global_index->at(colId[i - 1][j1]) == tmp[j0]) {
                    const OCP_DBL* begin = &val[i - 1][0] + j1 * blockSize;
                    const OCP_DBL* end = begin + blockSize;
                    copy(begin, end, &A[(iA[i - 1] + j0) * blockSize]);
                    break;
                }
            }
        }           
    }


    b = rhs.data();
    x = u.data();
}


OCP_INT PardisoSolver::Solve()
{

    /* -------------------------------------------------------------------- */
    /* .. Reordering and Symbolic Factorization. This step also allocates   */
    /* all memory that is necessary for the factorization.                  */
    /* -------------------------------------------------------------------- */


    phase = 11;
    cluster_sparse_solver(pt, &maxfct, &mnum, &mtype, &phase,
        &N, A.data(), iA.data(), jA.data(), &idum, &nrhs, iparm, &msglvl, &ddum, &ddum, &mycomm, &error);

    if (error != 0)
        OCP_ABORT("ERROR during symbolic factorization: " + to_string((long long int)error));

    /* -------------------------------------------------------------------- */
    /* .. Numerical factorization.                                          */
    /* -------------------------------------------------------------------- */
    phase = 22;
    cluster_sparse_solver(pt, &maxfct, &mnum, &mtype, &phase,
        &N, A.data(), iA.data(), jA.data(), &idum, &nrhs, iparm, &msglvl, &ddum, &ddum, &mycomm, &error);

    if (error != 0)
        OCP_ABORT("ERROR during numerical factorization: " + to_string((long long int)error));

    /* -------------------------------------------------------------------- */
    /* .. Back substitution and iterative refinement.                       */
    /* -------------------------------------------------------------------- */
    phase = 33;
    cluster_sparse_solver(pt, &maxfct, &mnum, &mtype, &phase,
        &N, A.data(), iA.data(), jA.data(), &idum, &nrhs, iparm, &msglvl, b, x, &mycomm, &error);

    if (error != 0)
        OCP_ABORT("ERROR during numerical solution: " + to_string((long long int)error));

    /* -------------------------------------------------------------------- */
    /* .. Termination and release of memory. */
    /* -------------------------------------------------------------------- */
    phase = -1; /* Release internal memory. */
    cluster_sparse_solver(pt, &maxfct, &mnum, &mtype, &phase,
        &N, &ddum, iA.data(), jA.data(), &idum, &nrhs, iparm, &msglvl, &ddum, &ddum, &mycomm, &error);

    if (error != 0)
        OCP_ABORT("ERROR during release memory: " + to_string((long long int)error));

    return 1;
}


/// Allocate memoery for pardiso solver
void VectorPardisoSolver::Allocate(const OCP_USI& max_nnz,
    const OCP_USI& maxDim,
    const USI& blockDim)
{
    blockdim = blockDim;
    iA.resize(maxDim * blockdim + 1);
    jA.resize(max_nnz * blockdim * blockdim);
    A.resize(max_nnz * blockdim * blockdim);
}


/// Calculate terms used in communication
void VectorPardisoSolver::CalCommTerm(const USI& actWellNum, const Domain* domain)
{
    global_index = domain->CalGlobalIndex(actWellNum);

    const OCP_USI numGridInterior = domain->GetNumGridInterior();
    const OCP_USI numElementLoc = actWellNum + numGridInterior;
    const OCP_USI global_end = global_index->at(numElementLoc - 1);

    iparm[40] = (global_end - numElementLoc + 1) * blockdim;  // global begin (included)
    iparm[41] = (global_end + 1) * blockdim - 1;              // global end   (included)

    // Get Dimension
    N = (global_end + 1) * blockdim;
    MPI_Bcast(&N, 1, MPI_INT, domain->numproc - 1, domain->myComm);
}


/// Assemble coefficient matrix.
void VectorPardisoSolver::AssembleMat(const vector<vector<USI>>& colId,
    const vector<vector<OCP_DBL>>& val,
    const OCP_USI& dim,
    vector<OCP_DBL>& rhs,
    vector<OCP_DBL>& u)
{
    const USI blockSize = blockdim * blockdim;
    vector<USI> tmp;
    // Assemble iA, jA, A
    iA[0] = 0;
    for (OCP_USI i = 1; i < dim + 1; i++) {
        const USI nnzR = colId[i - 1].size();
        const OCP_USI bId = (i - 1) * blockdim;

        for (USI c = 0; c < blockdim; c++)
            iA[bId + c + 1] = iA[bId + c] + nnzR * blockdim;
       
        // reorder
        tmp = colId[i - 1];
        for (auto& t : tmp)    t = global_index->at(t);
        sort(tmp.begin(), tmp.end());
        for (USI j0 = 0; j0 < nnzR; j0++) {

            for (USI c = 0; c < blockdim; c++)
                for (USI c1 = 0; c1 < blockdim; c1++)
                    jA[iA[bId + c] + j0 * blockdim + c1] = tmp[j0] * blockdim + c1;


            for (USI j1 = 0; j1 < nnzR; j1++) {
                if (global_index->at(colId[i - 1][j1]) == tmp[j0]) {

                    for (USI c = 0; c < blockdim; c++) {
                        const OCP_DBL* begin = &val[i - 1][0] + j1 * blockSize + c * blockdim;
                        const OCP_DBL* end = begin + blockdim;
                        copy(begin, end, &A[iA[bId + c] + j0 * blockdim]);
                    }
                        
                    break;
                }
            }
        }
    }

    //if (myrank == -1) {
    //    ofstream myFile;
    //    myFile.open("test/test" + to_string(myrank) + ".out");
    //    ios::sync_with_stdio(false);
    //    myFile.tie(0);

    //    myFile << dim * blockdim << endl;
    //    for (OCP_USI i = 0; i <= dim * blockdim; i++) {
    //        myFile << iA[i] << endl;
    //    }
    //    for (OCP_USI i = 0; i < dim * blockdim; i++) {
    //        for (OCP_USI j = iA[i]; j < iA[i + 1]; j++) {
    //            myFile << jA[j] << endl;
    //        }
    //    }
    //    for (OCP_USI i = 0; i < dim * blockdim; i++) {
    //        for (OCP_USI j = iA[i]; j < iA[i + 1]; j++) {
    //            myFile << A[j] << endl;
    //        }
    //    }

    //    myFile.close();
    //}

    b = rhs.data();
    x = u.data();
}



 /*----------------------------------------------------------------------------*/
 /*  Brief Change History of This File                                         */
 /*----------------------------------------------------------------------------*/
 /*  Author              Date             Actions                              */
 /*----------------------------------------------------------------------------*/
 /*  Shizhe Li           Mar/30/2023      Create file                          */
 /*----------------------------------------------------------------------------*/
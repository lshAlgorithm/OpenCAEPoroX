/*! \file    LinearSystem.hpp
 *  \brief   Linear solver class declaration
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __LINEARSYSTEM_HEADER__
#define __LINEARSYSTEM_HEADER__

// Standard header files
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// OpenCAEPoroX header files
#include "OCPConst.hpp"
#include "Domain.hpp"
#include "OCPControlMethod.hpp"
#include "DenseMat.hpp"
#include "PardisoSolver.hpp"
#include "SamgSolver.hpp"
#include "FaspSolver.hpp"
#include "PetscSolver.hpp"
#include "LinearSolver.hpp"

using namespace std;

/// Linear solvers for discrete systems.
//  Note: The matrix is stored in the form of row-segmented CSR internally
class LinearSystem
{

public:
    /// Setup linear system
    USI Setup(const OCPModel& model, const string& dir, const string& file, const Domain& d, const USI& nb);
    /// Set work LS
    void SetWorkLS(const USI& i) { wIndex = i; };
    /// Clear the internal matrix data for scalar-value problems.
    void ClearData();

    /// Return the solution.
    vector<OCP_DBL>& GetSolution() { return u; }
    /// Check whether NAN or INF occurs in equations, used in debug mode.
    void CheckEquation() const;
    /// Check whether NAN or INF occurs in solutions, used in debug mode.
    void CheckSolution() const;
    /// Output the mat and rhs to fileA and fileb. // TODO: output to some obj?
    void OutputLinearSystem(const string& fileA, const string& fileb) const;
    /// Output the solution to a disk file name.
    void OutputSolution(const string& filename) const;

    // Linear Solver
    /// Assemble Mat for Linear Solver.
    void AssembleMatLinearSolver() { LS[wIndex]->AssembleMat(colId, val, dim, b, u); }
    /// Solve the Linear System.
    OCP_INT Solve();

    /// Setup dimensions.
    OCP_USI AddDim(const OCP_USI& n)
    {
        dim += n;
        return dim;
    }

    // Scalar
    /// Push back a diagonal val, which is always at the first location.
    void NewDiag(const OCP_USI& n, const OCP_DBL& v)
    {
        OCP_ASSERT(colId[n].size() == 0, "Wrong Diag");
        colId[n].push_back(n);
        val[n].push_back(v);
    }
    /// Add a value at diagonal value.
    void AddDiag(const OCP_USI& n, const OCP_DBL& v)
    {
        OCP_ASSERT(colId[n].size() > 0, "Wrong Diag");
        val[n][0] += v;
    }
    /// Push back a off-diagonal value.
    void NewOffDiag(const OCP_USI& bId, const OCP_USI& eId, const OCP_DBL& v)
    {
        OCP_ASSERT(colId[bId].size() > 0, "Wrong Diag");
        colId[bId].push_back(eId);
        val[bId].push_back(v);
    }
    /// Add a value at b[n].
    void AddRhs(const OCP_USI& n, const OCP_DBL& v) { 
        b[n] += v; 
    }
    /// Assign an initial value at u[n].
    void AssignGuess(const OCP_USI& n, const OCP_DBL& v) { u[n] = v; }

    // Vector
    void NewDiag(const OCP_USI& n, const vector<OCP_DBL>& v)
    {
        OCP_ASSERT(colId[n].size() == 0, "Wrong Diag");
        colId[n].push_back(n);
        val[n].insert(val[n].begin(), v.begin(), v.end());
    }
    void AddDiag(const OCP_USI& n, const vector<OCP_DBL>& v)
    {
        OCP_ASSERT(colId[n].size() > 0, "Wrong Diag");
        for (USI i = 0; i < blockSize; i++) {
            val[n][i] += v[i];
        }
    }
    void NewOffDiag(const OCP_USI& bId, const OCP_USI& eId, const vector<OCP_DBL>& v)
    {
        OCP_ASSERT(colId[bId].size() > 0, "Wrong Diag");
        colId[bId].push_back(eId);
        val[bId].insert(val[bId].end(), v.begin(), v.end());
    }
    /// Add a value at b[n].
    void AddRhs(const OCP_USI& n, const vector<OCP_DBL>& v)
    {
        for (USI i = 0; i < blockDim; i++) {
            b[n * blockDim + i] += v[i];
        }
    }

    /// Assign Rhs by Accumulating.
    void AssembleRhsAccumulate(const vector<OCP_DBL>& rhs);
    /// Assign Rhs by Copying.
    void AssembleRhsCopy(const vector<OCP_DBL>& rhs);

    /// Calculate Global start
    void CalCommTerm(const USI& actWellNum) {
        LS[wIndex]->CalCommTerm(actWellNum, domain);
    }


protected:
    /// Allocate memory for linear system with max possible number of rows.
    void AllocateRowMem();
    /// Allocate memory for linear system with max possible number of columns.
    void AllocateColMem();
    /// Setup LinearSolver.
    void SetupLinearSolver(const OCPModel& model, const string& lsFile);

protected:
    /// working index of LS
    USI wIndex;
    // Used for internal mat structure.
    /// Dimens of small block matrix.
    USI blockDim;
    /// Size of small block matrix.
    USI blockSize; 

    // maxDim: fixed and used to allocate memory at the beginning of simulation;
    // dim:    might change during simulation but always less than maxDim.
    /// Maximal possible dimension of matrix.
    OCP_USI maxDim; 
    /// Actual dimension of matrix.
    OCP_USI dim;    
    /// maximum nnz
    OCP_USI max_nnz; 

    // The following values are stored for each row.
    /// Column indices of nonzero entry.
    vector<vector<OCP_USI>> colId;       
    /// Nonzero values.
    vector<vector<OCP_DBL>> val;         
    /// Right-hand side of linear system.
    vector<OCP_DBL>         b;           
    /// Solution of linear system.
    vector<OCP_DBL>         u;           
    /// Current workdir.
    string    solveDir; 
    
    const Domain* domain;

    /// LStype sets
    vector<OCPLStype>     LStype;
    /// LS sets
    vector<LinearSolver*> LS;

};

#endif /* end if __LINEARSOLVER_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/15/2021      Format file                          */
/*  Chensong Zhang      Nov/09/2021      Remove decoupling methods            */
/*  Chensong Zhang      Nov/22/2021      renamed to LinearSystem              */
/*----------------------------------------------------------------------------*/
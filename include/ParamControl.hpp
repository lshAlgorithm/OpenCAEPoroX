/*! \file    ParamControl.hpp
 *  \brief   ParamControl class declaration
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __PARAMCONTROL_HEADER__
#define __PARAMCONTROL_HEADER__

// Standard header files
#include <cassert>
#include <fstream>
#include <vector>

// OpenCAEPoroX header files
#include "OCPConst.hpp"
#include "UtilInput.hpp"
#include "UtilOutput.hpp"

/// Tuning is a set of param of control, which contains three main parts
/// 1. Time stepping controls.
/// 2. Time truncation and convergence controls.
/// 3. Newton and linear iterations controls.
/// But most of these have not been applied to current program.
typedef vector<vector<OCP_DBL>> TUNING;

/// TODO: Add Doxygen
class TuningPair
{
public:
    TuningPair(const USI& t, const TUNING& tmp)
        : d(t)
        , Tuning(tmp){};
    USI    d;
    TUNING Tuning;
};

/// ParamControl contains the param referred to control of simulation, for example,
/// which discrete method will be used, which linear solve file will be used, how will
/// the time step change.
class ParamControl
{
public:
    /// model: thermal or isothermal.
    OCPModel           model{OCPModel::isothermal };
    /// Current work directory.
    string             workDir;   
    /// Current file name
    string             fileName;               
    /// Discretization method for fluid equations.
    string             method;    
    /// linear solver input file.
    string             lsFile; 
    /// Tuning set.
    vector<TuningPair> tuning_T;  
    /// Tuning.
    TUNING             tuning;      
    /// Max simulation time
    OCP_DBL            MaxSimTime{ 1E20 };

    /// Critical time records the important time points, at those times, the process of
    /// simulation should be carefully treated, for example, the boundary conditions
    /// will be changed.
    vector<OCP_DBL> criticalTime;

    /// Assign default values to parameters.
    void Init(string& indir, string& inFileName);
    /// Init the critical time.
    void InitTime() { criticalTime.push_back(0); };
    /// Determine the default discrete method.
    void InitMethod();
    /// Determine the default Tuning.
    void InitTuning();
    /// Input the Keyword: METHOD.
    void InputMETHOD(ifstream& ifs);
    /// Input Maximum simulation time
    void InpuMaxSimTime(ifstream& ifs);
    /// Input the Keyword: TUNING.
    void InputTUNING(ifstream& ifs);
    /// Display the Tuning.
    void DisplayTuning() const;
};

#endif /* end if __ParamControl_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/15/2021      Format file                          */
/*----------------------------------------------------------------------------*/
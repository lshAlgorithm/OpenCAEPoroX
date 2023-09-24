/*! \file    Solver.cpp
 *  \brief   Solver class definition
 *  \author  Shizhe Li
 *  \date    Oct/21/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

// OpenCAEPoroX header files
#include "Solver.hpp"

void Solver::Setup(Reservoir& rs, const OCPControl& ctrl)
{

    model = ctrl.GetModel();
    switch (model) 
    {
        case OCPModel::isothermal:
            SetupIsoT(rs, ctrl);
            break;
        case OCPModel::thermal:
            SetupT(rs, ctrl);
            break;
        default:
            OCP_ABORT("Wrong model type specified!");
            break;
    }
}

void Solver::SetupIsoT(Reservoir& rs, const OCPControl& ctrl)
{
    // Setup static infomation for reservoir
    rs.Setup();
    IsoTSolver.SetupMethod(rs, ctrl);
}

void Solver::SetupT(Reservoir& rs, const OCPControl& ctrl)
{
    rs.Setup();
    TSolver.SetupMethod(rs, ctrl);
}

/// Initialize the reservoir setting for different solution methods.
void Solver::InitReservoir(Reservoir& rs)
{
    switch (model) 
    {
        case OCPModel::isothermal:
            IsoTSolver.InitReservoir(rs);
            break;
        case OCPModel::thermal:
            TSolver.InitReservoir(rs);
            break;
        default:
            OCP_ABORT("Wrong model type specified!");
            break;
    }
}

/// Simulation will go through all time steps and call GoOneStep at each step.
void Solver::RunSimulation(Reservoir& rs, OCPControl& ctrl, OCPOutput& output)
{
    GetWallTime timer;
    timer.Start();
    output.PrintInfoSched(rs, ctrl, timer.Stop());
    USI numTSteps = ctrl.GetNumTSteps();
    for (USI d = 0; d < numTSteps - 1; d++) {
        rs.ApplyControl(d);
        ctrl.ApplyControl(d, rs);
        while (!ctrl.IsCriticalTime(d + 1)) {
            GoOneStep(rs, ctrl);
            output.SetVal(rs, ctrl);
            if (ctrl.printLevel >= PRINT_ALL) {
                // Print Summary and critical information at every time step
                output.PrintInfo();
            }
        }
        output.PrintInfoSched(rs, ctrl, timer.Stop());
        // rs.allWells.ShowWellStatus(rs.bulk);
    }
    rs.OutInfoFinal();
    OCPTIME_TOTAL += timer.Stop() / 1000;
}

/// This is one time step of dynamic simulation in an abstract setting.
void Solver::GoOneStep(Reservoir& rs, OCPControl& ctrl)
{

    if (ctrl.printLevel >= PRINT_SOME && CURRENT_RANK == MASTER_PROCESS) {
        cout << "### DEBUG: " << setprecision(3) << fixed << ctrl.GetCurTime()
             << " Days";
        cout << ",  NR: " << ctrl.iters.GetNRt() << ",  LS: " << ctrl.iters.GetLSt()
             << ",  Last dt: " << ctrl.GetLastDt() << " Days" << endl;
    }

    switch (model) 
    {
        case OCPModel::isothermal:
            GoOneStepIsoT(rs, ctrl);
            break;
        case OCPModel::thermal:
            GoOneStepT(rs, ctrl);
            break;
        default:
            OCP_ABORT("Wrong model type specified!");
            break;
    }
}

void Solver::GoOneStepIsoT(Reservoir& rs, OCPControl& ctrl)
{
    // Prepare for time marching
    IsoTSolver.Prepare(rs, ctrl);
    
    // Time marching with adaptive time stepsize
    while (OCP_TRUE) {
        if (ctrl.GetCurDt() < MIN_TIME_CURSTEP)
            OCP_ABORT("Time stepsize is too small!");
        // Assemble linear system
        IsoTSolver.AssembleMat(rs, ctrl);
        // Solve linear system
        IsoTSolver.SolveLinearSystem(rs, ctrl);
        if (!IsoTSolver.UpdateProperty(rs, ctrl)) {
            continue;
        }
        if (IsoTSolver.FinishNR(rs, ctrl)) break;
    }

    // Finish current time step
    IsoTSolver.FinishStep(rs, ctrl);
}

void Solver::GoOneStepT(Reservoir& rs, OCPControl& ctrl)
{
    // Prepare for time marching
    TSolver.Prepare(rs, ctrl);

    // Time marching with adaptive time stepsize
    while (OCP_TRUE) {
        if (ctrl.GetCurDt() < MIN_TIME_CURSTEP)
            OCP_ABORT("Time stepsize is too small!");
        // Assemble linear system
        TSolver.AssembleMat(rs, ctrl);
        // Solve linear system
        TSolver.SolveLinearSystem(rs, ctrl);
        if (!TSolver.UpdateProperty(rs, ctrl)) {
            continue;
        }
        if (TSolver.FinishNR(rs, ctrl)) break;
    }

    // Finish current time step
    TSolver.FinishStep(rs, ctrl);
}

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/21/2021      Create file                          */
/*----------------------------------------------------------------------------*/
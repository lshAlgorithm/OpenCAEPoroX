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
/*
    Whole thing  is here! All about the simulation! -- Li Shuhuai
*/
void Solver::RunSimulation(Reservoir& rs, OCPControl& ctrl, OCPOutput& output)
{
    GetWallTime timer;
    timer.Start();
    output.PrintInfoSched(rs, ctrl, timer.Stop());
    for (USI d = 0; d < ctrl.time.GetNumTstepInterval(); d++) {
        rs.ApplyControl(d);
        ctrl.ApplyControl(d, rs);
        while (!ctrl.time.IfEnd()) {
            output.PrintCurrentTimeIter(ctrl);

            /*
                Core func! -- Li Shuhuai
            */
            const OCPNRsuite& NR = GoOneStep(rs, ctrl);

            cerr << "===========YES YES YES, Go one step!==================\t";
            output.SetVal(rs, ctrl, NR);
            if (ctrl.printLevel >= PRINT_ALL) {
                // Print Summary and critical information at every time step
                output.PrintInfo();
            }

            if (timer.Stop() > ctrl.MaxSimTime * TIME_S2MS) ctrl.StopSim = OCP_TRUE;
            if (ctrl.StopSim)  break;
        }
        output.PrintInfoSched(rs, ctrl, timer.Stop());
        // rs.allWells.ShowWellStatus(rs.bulk);     
        if (ctrl.StopSim)        break;
    }
    rs.OutInfoFinal();
    OCPTIME_TOTAL += timer.Stop() / TIME_S2MS;
}

/// This is one time step of dynamic simulation in an abstract setting.
const OCPNRsuite& Solver::GoOneStep(Reservoir& rs, OCPControl& ctrl)
{
    switch (model) 
    {
        case OCPModel::isothermal:
            return GoOneStepIsoT(rs, ctrl);
        case OCPModel::thermal:
            return GoOneStepT(rs, ctrl);
            break;
        default:
            OCP_ABORT("Wrong model type specified!");
            break;
    }
}

const OCPNRsuite& Solver::GoOneStepIsoT(Reservoir& rs, OCPControl& ctrl)
{
    // Prepare for time marching
    IsoTSolver.Prepare(rs, ctrl);
    cerr << "===========Preparation finished!!!!!==================\t";
    /*
        UPDATE STEP BY STEP -- Li Shuhuai    
    */
    // Time marching with adaptive time stepsize

    // #pragma omp parallel
    // {
        int cnt = 0;
        while (OCP_TRUE) {
            #pragma omp single
            {
                cnt++;
                printf("the number the go one step iter is %d\t", cnt);
            }
            if (ctrl.time.GetCurrentDt() < MIN_TIME_CURSTEP) {
                if(CURRENT_RANK == MASTER_PROCESS)
                    OCP_WARNING("Time stepsize is too small: " + to_string(ctrl.time.GetCurrentDt()) + TIMEUNIT);
                ctrl.StopSim = OCP_TRUE;
                break;
            }

            // Assemble linear system
            #pragma omp parallel
            {
                IsoTSolver.AssembleMat(rs, ctrl);
            }
            cerr << "===========AssembleMat==================\t";
            // Solve linear system


            IsoTSolver.SolveLinearSystem(rs, ctrl);
            cerr << "===========SolveLinearSystem==================\t";
            if (!IsoTSolver.UpdateProperty(rs, ctrl)) {
                continue;
            }
            cerr << "===========updateProperty==================\t";
            if (IsoTSolver.FinishNR(rs, ctrl)) break;
        }
    // }

    // Finish current time step
    cerr << "===========OCP Iteration finished!==================\t";
    IsoTSolver.FinishStep(rs, ctrl);

    return IsoTSolver.GetNRsuite();
}

const OCPNRsuite& Solver::GoOneStepT(Reservoir& rs, OCPControl& ctrl)
{
    // Prepare for time marching
    TSolver.Prepare(rs, ctrl);

    // Time marching with adaptive time stepsize
    while (OCP_TRUE) {
        if (ctrl.time.GetCurrentDt() < MIN_TIME_CURSTEP) {
            if (CURRENT_RANK == MASTER_PROCESS)
                OCP_WARNING("Time stepsize is too small: " + to_string(ctrl.time.GetCurrentDt()) + TIMEUNIT);
            ctrl.StopSim = OCP_TRUE;
            break;
        }
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

    return TSolver.GetNRsuite();
}

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/21/2021      Create file                          */
/*----------------------------------------------------------------------------*/
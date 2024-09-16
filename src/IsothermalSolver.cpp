/*! \file    IsothermalSolver.cpp
 *  \brief   IsothermalSolver class definition
 *  \author  Shizhe Li
 *  \date    Oct/21/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "IsothermalSolver.hpp"

/// Setup solution methods, including IMPEC and FIM.
void IsothermalSolver::SetupMethod(Reservoir& rs, const OCPControl& ctrl)
{
    method = ctrl.GetMethod();

    switch (method) {
        case IMPEC:
            impec.Setup(rs, LSolver, ctrl);
            break;
        case FIM:
            fim.Setup(rs, LSolver, ctrl);
            break;
        case AIMc:
            aimc.Setup(rs, LSolver, ctrl);
            break;
        default:
            OCP_ABORT("Wrong method type!");
    }
}


/// Setup solution methods, including IMPEC and FIM.
void IsothermalSolver::InitReservoir(Reservoir& rs)
{
    switch (method) {
        case IMPEC:
            impec.InitReservoir(rs);
            break;
        case FIM:
            fim.InitReservoir(rs);
            break;
        case AIMc:
            aimc.InitReservoir(rs);
            break;
        default:
            OCP_ABORT("Wrong method type!");
    }
}


/// Prepare solution methods, including IMPEC and FIM.
void IsothermalSolver::Prepare(Reservoir& rs, OCPControl& ctrl)
{
    switch (method) {
        case IMPEC:
            impec.Prepare(rs, ctrl);
            break;
        case FIM:
            fim.Prepare(rs, ctrl.time.GetCurrentDt());
            break;
        case AIMc:
            aimc.Prepare(rs, ctrl.time.GetCurrentDt());
            break;
        default:
            OCP_ABORT("Wrong method type!");
    }
}


/// Assemble linear systems for IMPEC and FIM.
void IsothermalSolver::AssembleMat(const Reservoir& rs, OCPControl& ctrl)
{
    // Assemble
    const OCP_DBL dt = ctrl.time.GetCurrentDt();
    GetWallTime timer;
    timer.Start();

    switch (method) {
        case IMPEC:
            impec.AssembleMat(LSolver, rs, dt);
            break;
        case FIM:
            fim.AssembleMat(LSolver, rs, dt);
            break;
        case AIMc:
            aimc.AssembleMat(LSolver, rs, dt);
            break;
        default:
            OCP_ABORT("Wrong method type!");
    }
   
    OCPTIME_ASSEMBLE_MAT += timer.Stop() / TIME_S2MS;
}


/// Solve linear systems for IMPEC and FIM.
void IsothermalSolver::SolveLinearSystem(Reservoir& rs, OCPControl& ctrl)
{ 
    switch (method) {
        case IMPEC:
            impec.SolveLinearSystem(LSolver, rs, ctrl);
            break;
        case FIM:
            fim.SolveLinearSystem(LSolver, rs, ctrl);
            break;
        case AIMc:
            aimc.SolveLinearSystem(LSolver, rs, ctrl);
            break;
        default:
            OCP_ABORT("Wrong method type!");
    }
}


/// Update physical properties for IMPEC and FIM.
OCP_BOOL IsothermalSolver::UpdateProperty(Reservoir& rs, OCPControl& ctrl)
{
    OCP_BOOL flag;

    GetWallTime timer;
    timer.Start();

    switch (method) {
        case IMPEC:
            flag = impec.UpdateProperty(rs, ctrl);
            break;
        case FIM:
            flag = fim.UpdateProperty(rs, ctrl);
            break;
        case AIMc:
            flag = aimc.UpdateProperty(rs, ctrl);
            break;
        default:
            OCP_ABORT("Wrong method type!");
    }

    OCPTIME_UPDATE_GRID += timer.Stop() / TIME_S2MS;

    return flag;
}


/// Finish up Newton-Raphson iteration for IMPEC and FIM.
OCP_BOOL IsothermalSolver::FinishNR(Reservoir& rs, OCPControl& ctrl)
{
    switch (method) {
        case IMPEC:
            return impec.FinishNR(rs);
        case FIM:
            return fim.FinishNR(rs, ctrl);
        case AIMc:
            return aimc.FinishNR(rs, ctrl);
        default:
            OCP_ABORT("Wrong method type!");
    }
}


/// Finish up time step for IMPEC and FIM.
void IsothermalSolver::FinishStep(Reservoir& rs, OCPControl& ctrl)
{
    switch (method) {
        case IMPEC:
            impec.FinishStep(rs, ctrl);
            break;
        case FIM:
            fim.FinishStep(rs, ctrl);
            break;
        case AIMc:
            aimc.FinishStep(rs, ctrl);
            break;
        default:
            OCP_ABORT("Wrong method type!");
    }
}


const OCPNRsuite& IsothermalSolver::GetNRsuite() const
{
    switch (method) {
    case IMPEC:
        return impec.GetNRsuite();
        break;
    case FIM:
        return fim.GetNRsuite();
        break;
    case AIMc:
        return aimc.GetNRsuite();
        break;
    default:
        OCP_ABORT("Wrong method type!");
    }
}

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/21/2021      Create file                          */
/*  Chensong Zhang      Jan/08/2022      Update Doxygen                       */
/*----------------------------------------------------------------------------*/
/*! \file    ThermalMethod.cpp
 *  \brief   ThermalMethod class declaration
 *  \author  Shizhe Li
 *  \date    Nov/10/2022
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "ThermalMethod.hpp"

void T_FIM::Setup(Reservoir& rs, LinearSystem& ls, const OCPControl& ctrl)
{
    AllocateReservoir(rs);
    AllocateLinearSystem(ls, rs, ctrl);
}

void T_FIM::InitReservoir(Reservoir& rs)
{
    rs.bulk.Initialize(rs.domain);

    InitRock(rs.bulk);
    CalRock(rs.bulk);

    InitFlash(rs.bulk);
    CalKrPc(rs.bulk);

    CalThermalConduct(rs.conn, rs.bulk);

    rs.allWells.InitBHP(rs.bulk);
    UpdateLastTimeStep(rs);
}

void T_FIM::Prepare(Reservoir& rs, const OCPControl& ctrl)
{
    rs.allWells.PrepareWell(rs.bulk);
    CalRes(rs, ctrl.GetCurTime() + ctrl.GetCurDt(), ctrl.GetCurDt(), OCP_TRUE);
}

void T_FIM::AssembleMat(LinearSystem&    ls,
                        const Reservoir& rs,
                        const OCP_DBL&   t,
                        const OCP_DBL&   dt)
{
    AssembleMatBulks(ls, rs, t, dt);
    AssembleMatWells(ls, rs, dt);
    ls.AssembleRhsCopy(res.resAbs);
}

void T_FIM::SolveLinearSystem(LinearSystem& ls, Reservoir& rs, OCPControl& ctrl)
{
#ifdef DEBUG
    ls.CheckEquation();
#endif // DEBUG

    GetWallTime timer;

    // Assemble external linear solver with internal A and b
    timer.Start();
    ls.CalCommTerm(rs.GetNumOpenWell());
    ls.AssembleMatLinearSolver();
    OCPTIME_ASSEMBLE_MAT_FOR_LS += timer.Stop() / 1000;

    // Solve linear system  
    timer.Start();
    int status = ls.Solve();
    if (status < 0) {
        status = ls.GetNumIters();
    }
    // Record time, iterations
    OCPTIME_LSOLVER += timer.Stop() / 1000;
    ctrl.UpdateIterLS(status);
    ctrl.UpdateIterNR();

#ifdef DEBUG
    // Output A, b, x
     //ls.OutputLinearSystem("testA_FIMT.out", "testb_FIMT.out");
     //ls.OutputSolution("testx_FIMT.out");
    // Check if inf or nan occurs in solution
    ls.CheckSolution();
#endif // DEBUG
    
    timer.Start();
    GetSolution(rs, ls.GetSolution(), ctrl);
    OCPTIME_NRSTEP += timer.Stop() / 1000;
    // rs.PrintSolFIM(ctrl.workDir + "testPNi.out");
    ls.ClearData();
}

OCP_BOOL T_FIM::UpdateProperty(Reservoir& rs, OCPControl& ctrl)
{
    OCP_DBL& dt = ctrl.current_dt;

    if (!ctrl.Check(rs, {"BulkNi", "BulkP", "BulkT"})) {
        ResetToLastTimeStep(rs, ctrl);
        if (CURRENT_RANK == MASTER_PROCESS) {
            cout << "Cut time step size and repeat! current dt = " << fixed
                << setprecision(3) << dt << " days\n";
        }      
        return OCP_FALSE;
    }

    // Update reservoir properties
    CalRock(rs.bulk);

    CalFlash(rs.bulk);
    CalKrPc(rs.bulk);

    CalThermalConduct(rs.conn, rs.bulk);
    
    rs.bulk.BCm.CalHeatLoss(rs.bulk.vs, ctrl.GetCurTime() + dt, dt);

    rs.allWells.CalTrans(rs.bulk);
    rs.allWells.CalFlux(rs.bulk);

    CalRes(rs, ctrl.GetCurTime() + dt, dt, OCP_FALSE);

    return OCP_TRUE;
}

OCP_BOOL T_FIM::FinishNR(Reservoir& rs, OCPControl& ctrl)
{

    NRdSmax = CalNRdSmax();
    // const OCP_DBL NRdNmax = rs.GetNRdNmax();

    OCP_INT conflag_loc = -1;
    if (((res.maxRelRes_V <= res.maxRelRes0_V * ctrl.ctrlNR.NRtol ||
        res.maxRelRes_V <= ctrl.ctrlNR.NRtol ||
        res.maxRelRes_N <= ctrl.ctrlNR.NRtol) &&
        res.maxWellRelRes_mol <= ctrl.ctrlNR.NRtol) ||
        (fabs(NRdPmax) <= ctrl.ctrlNR.NRdPmin &&
            fabs(NRdSmax) <= ctrl.ctrlNR.NRdSmin)) {
        conflag_loc = 0;
    }

    GetWallTime timer;
    timer.Start();

    OCP_INT conflag;
    MPI_Allreduce(&conflag_loc, &conflag, 1, MPI_INT, MPI_MIN, rs.domain.myComm);

    OCPTIME_COMM_COLLECTIVE += timer.Stop() / 1000;


    if (conflag == 0) {

        if (!ctrl.Check(rs, {"WellP"})) {
            ResetToLastTimeStep(rs, ctrl);
            return OCP_FALSE;
        } else {
            return OCP_TRUE;
        }

    } else if (ctrl.iterNR >= ctrl.ctrlNR.maxNRiter) {
        ctrl.current_dt *= ctrl.ctrlTime.cutFacNR;
        ResetToLastTimeStep(rs, ctrl);
        cout << "### WARNING: NR not fully converged! Cut time step size and repeat!  "
                "current dt = "
             << fixed << setprecision(3) << ctrl.current_dt << " days\n";
        return OCP_FALSE;
    } else {
        return OCP_FALSE;
    }
}

void T_FIM::FinishStep(Reservoir& rs, OCPControl& ctrl)
{
    rs.CalIPRT(ctrl.GetCurDt());
    rs.CalMaxChange();
    UpdateLastTimeStep(rs);
    ctrl.CalNextTimeStep(rs, {"dP", "dS", "iter"});
    ctrl.UpdateIters();
}

void T_FIM::AllocateReservoir(Reservoir& rs)
{
    Bulk&         bk = rs.bulk;
    BulkVarSet&   bvs = bk.vs;
    const OCP_USI nb = bvs.nb;
    const USI     np = bvs.np;
    const USI     nc = bvs.nc;

    // Rock
    bvs.poro.resize(nb);
    bvs.rockVp.resize(nb);
    bvs.vr.resize(nb);
    bvs.Hr.resize(nb);

    bvs.lporo.resize(nb);
    bvs.lrockVp.resize(nb);
    bvs.lvr.resize(nb);
    bvs.lHr.resize(nb);

    // derivatives
    bvs.poroP.resize(nb);
    bvs.poroT.resize(nb);
    bvs.vrP.resize(nb);
    bvs.vrT.resize(nb);
    bvs.HrT.resize(nb);

    bvs.lporoP.resize(nb);
    bvs.lporoT.resize(nb);
    bvs.lvrP.resize(nb);
    bvs.lvrT.resize(nb);
    bvs.lHrT.resize(nb);

    // Fluid
    bvs.phaseNum.resize(nb);
    bvs.Nt.resize(nb);
    bvs.Ni.resize(nb * nc);
    bvs.vf.resize(nb);
    bvs.initT.resize(nb);
    bvs.T.resize(nb);
    bvs.P.resize(nb);
    bvs.Pb.resize(nb);
    bvs.Pj.resize(nb * np);
    bvs.Pc.resize(nb * np);
    bvs.phaseExist.resize(nb * np);
    bvs.S.resize(nb * np);
    bvs.xij.resize(nb * np * nc);
    bvs.rho.resize(nb * np);
    bvs.xi.resize(nb * np);
    bvs.mu.resize(nb * np);
    bvs.kr.resize(nb * np);
    bvs.Uf.resize(nb);
    bvs.H.resize(nb * np);
    bvs.kt.resize(nb);

    bvs.lphaseNum.resize(nb);
    bvs.lNt.resize(nb);
    bvs.lNi.resize(nb * nc);
    bvs.lvf.resize(nb);
    bvs.lT.resize(nb);
    bvs.lP.resize(nb);
    bvs.lPj.resize(nb * np);
    bvs.lPc.resize(nb * np);
    bvs.lphaseExist.resize(nb * np);
    bvs.lS.resize(nb * np);
    bvs.lxij.resize(nb * np * nc);
    bvs.lrho.resize(nb * np);
    bvs.lxi.resize(nb * np);
    bvs.lmu.resize(nb * np);
    bvs.lkr.resize(nb * np);
    bvs.lUf.resize(nb);
    bvs.lH.resize(nb * np);
    bvs.lkt.resize(nb);

    // derivatives
    bvs.vfP.resize(nb);
    bvs.vfT.resize(nb);
    bvs.vfi.resize(nb * nc);
    bvs.rhoP.resize(nb * np);
    bvs.rhoT.resize(nb * np);
    bvs.rhox.resize(nb * nc * np);
    bvs.xiP.resize(nb * np);
    bvs.xiT.resize(nb * np);
    bvs.xix.resize(nb * nc * np);
    bvs.muP.resize(nb * np);
    bvs.muT.resize(nb * np);
    bvs.mux.resize(nb * nc * np);
    bvs.dPcdS.resize(nb * np * np);
    bvs.dKrdS.resize(nb * np * np);
    bvs.UfP.resize(nb);
    bvs.UfT.resize(nb);
    bvs.Ufi.resize(nb * nc);
    bvs.HT.resize(nb * np);
    bvs.Hx.resize(nb * np * nc);
    bvs.ktP.resize(nb);
    bvs.ktT.resize(nb);
    bvs.ktS.resize(nb * np);

    bvs.lvfP.resize(nb);
    bvs.lvfT.resize(nb);
    bvs.lvfi.resize(nb * nc);
    bvs.lrhoP.resize(nb * np);
    bvs.lrhoT.resize(nb * np);
    bvs.lrhox.resize(nb * nc * np);
    bvs.lxiP.resize(nb * np);
    bvs.lxiT.resize(nb * np);
    bvs.lxix.resize(nb * nc * np);
    bvs.lmuP.resize(nb * np);
    bvs.lmuT.resize(nb * np);
    bvs.lmux.resize(nb * nc * np);
    bvs.ldPcdS.resize(nb * np * np);
    bvs.ldKrdS.resize(nb * np * np);
    bvs.UfP.resize(nb);
    bvs.UfT.resize(nb);
    bvs.Ufi.resize(nb * nc);
    bvs.HT.resize(nb * np);
    bvs.Hx.resize(nb * np * nc);
    bvs.ktP.resize(nb);
    bvs.ktT.resize(nb);
    bvs.ktS.resize(nb * np);

    // FIM-Specified
    bvs.lendSdP = (nc + 2) * (nc + 1) * np;
    bvs.dSec_dPri.resize(nb * bvs.lendSdP);

    bvs.ldSec_dPri.resize(nb * bvs.lendSdP);

    // BulkConn
    BulkConn&      conn    = rs.conn;
    const OCP_USI& numConn = conn.numConn;

    conn.vs.upblock.resize(numConn* np);
    conn.vs.rho.resize(numConn* np);
    conn.vs.flux_vj.resize(numConn* np);


    // NR
    dSNR.resize(nb* np);
    dNNR.resize(nb* nc);
    dPNR.resize(nb);
    dTNR.resize(nb);

    // Allocate Residual
    res.SetupT(bvs.nbI, rs.allWells.numWell, nc);
}

void T_FIM::AllocateLinearSystem(LinearSystem&     ls,
                                 const Reservoir&  rs,
                                 const OCPControl& ctrl)
{
    ls.SetupDomain(rs.domain);
    ls.AllocateRowMem(rs.GetComNum() + 2);
    ls.AllocateColMem();
    ls.SetupLinearSolver(THERMALMODEL, ctrl.GetWorkDir(), ctrl.GetLsFile());
}

void T_FIM::InitRock(Bulk& bk) const
{
    BulkVarSet& bvs = bk.vs;
    for (OCP_USI n = 0; n < bvs.nb; n++) {
        if (bvs.cType[n] == BulkContent::r) {
            // non fluid bulk
            bvs.poroInit[n] = 0;
            bvs.poro[n]     = 0;
            bvs.rockVp[n]   = 0;
            bvs.vr[n]       = bvs.v[n];
        }
    }
}

void T_FIM::CalRock(Bulk& bk) const
{
    BulkVarSet&   bvs = bk.vs;
    const OCP_USI nb  = bvs.nb;

    for (OCP_USI n = 0; n < nb; n++) {
        auto ROCK = bk.ROCKm.GetROCK(n);

        ROCK->CalPoro(bvs.P[n], bvs.T[n], bvs.poroInit[n], bvs.cType[n]);
        if (bvs.cType[n] == BulkContent::rf) {
            // with fluid           
            bvs.poro[n]   = ROCK->GetPoro();
            bvs.poroP[n]  = ROCK->GetdPorodP();
            bvs.poroT[n]  = ROCK->GetdPorodT();           
            bvs.vr[n]     = bvs.v[n] * ROCK->Get_Poro();
            bvs.vrP[n]    = bvs.v[n] * ROCK->Get_dPorodP();
            bvs.vrT[n]    = bvs.v[n] * ROCK->Get_dPorodT();
            bvs.rockVp[n] = bvs.v[n] * bvs.poro[n];
        }
        bvs.Hr[n]  = ROCK->GetHr();
        bvs.HrT[n] = ROCK->GetdHrdT();
    }
}

void T_FIM::InitFlash(Bulk& bk)
{
    BulkVarSet&    bvs = bk.vs;
    const OCP_USI& nb = bvs.nb;
    const USI&     np = bvs.np;
    const USI&     nc = bvs.nc;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bvs.cType[n] == BulkContent::rf) {
            auto PVT = bk.PVTm.GetPVT(n);

            PVT->InitFlashFIM(bvs.P[n], bvs.Pb[n], bvs.T[n], &bvs.S[n * np],
                                            bvs.rockVp[n],&bvs.Ni[n * nc], n);
            for (USI i = 0; i < nc; i++) {
                bvs.Ni[n * nc + i] = PVT->GetNi(i);
            }
            PassFlashValue(bk, n);
        }
    }
}

void T_FIM::CalFlash(Bulk& bk)
{
    const BulkVarSet& bvs = bk.vs;
    const OCP_USI&    nb  = bvs.nb;
    const USI&        np  = bvs.np;
    const USI&        nc  = bvs.nc;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bvs.cType[n] == BulkContent::rf) {
            bk.PVTm.GetPVT(n)->FlashFIM(bvs.P[n], bvs.T[n], &bvs.Ni[n * nc], &bvs.S[n * np], 
                                        bvs.phaseNum[n], &bvs.xij[n * np * nc], n);
            PassFlashValue(bk, n);
        }
    }
}

void T_FIM::PassFlashValue(Bulk& bk, const OCP_USI& n)
{
    auto&         bvs  = bk.vs;
    const auto    PVT  = bk.PVTm.GetPVT(n);
    const auto&   np   = bvs.np;
    const auto&   nc   = bvs.nc;
    const OCP_USI bIdp = n * np;

    bvs.phaseNum[n] = 0;
    bvs.Nt[n]       = PVT->GetNt();
    bvs.vf[n]       = PVT->GetVf();
    bvs.Uf[n]       = PVT->GetUf();

    for (USI j = 0; j < np; j++) {
        // Important! Saturation must be passed no matter if the phase exists. This is
        // because it will be used to calculate relative permeability and capillary
        // pressure at each time step. Make sure that all saturations are updated at
        // each step!
        bvs.S[bIdp + j] = PVT->GetS(j);
        dSNR[bIdp + j] = bvs.S[bIdp + j] - dSNR[bIdp + j];
        bvs.phaseExist[bIdp + j] = PVT->GetPhaseExist(j);
        if (bvs.phaseExist[bIdp + j]) {
            bvs.phaseNum[n]++;
            bvs.rho[bIdp + j] = PVT->GetRho(j);
            bvs.xi[bIdp + j]  = PVT->GetXi(j);
            bvs.mu[bIdp + j]  = PVT->GetMu(j);
            bvs.H[bIdp + j]   = PVT->GetH(j);

            // Derivatives
            bvs.rhoP[bIdp + j] = PVT->GetRhoP(j);
            bvs.rhoT[bIdp + j] = PVT->GetRhoT(j);
            bvs.xiP[bIdp + j]  = PVT->GetXiP(j);
            bvs.xiT[bIdp + j]  = PVT->GetXiT(j);
            bvs.muP[bIdp + j]  = PVT->GetMuP(j);
            bvs.muT[bIdp + j]  = PVT->GetMuT(j);
            bvs.HT[bIdp + j]   = PVT->GetHT(j);

            for (USI i = 0; i < nc; i++) {
                bvs.xij[bIdp * nc + j * nc + i]  = PVT->GetXij(j, i);
                bvs.rhox[bIdp * nc + j * nc + i] = PVT->GetRhoX(j, i);
                bvs.xix[bIdp * nc + j * nc + i]  = PVT->GetXiX(j, i);
                bvs.mux[bIdp * nc + j * nc + i]  = PVT->GetMuX(j, i);
                bvs.Hx[bIdp * nc + j * nc + i]   = PVT->GetHx(j, i);
            }
        }
    }
    bvs.vfP[n] = PVT->GetVfP();
    bvs.vfT[n] = PVT->GetVfT();
    bvs.UfP[n] = PVT->GetUfP();
    bvs.UfT[n] = PVT->GetUfT();

    for (USI i = 0; i < nc; i++) {
        bvs.vfi[n * nc + i] = PVT->GetVfi(i);
        bvs.Ufi[n * nc + i] = PVT->GetUfi(i);
    }

    Dcopy(bvs.lendSdP, &bvs.dSec_dPri[n * bvs.lendSdP],
          &PVT->GetDXsDXp()[0]);
}

void T_FIM::CalKrPc(Bulk& bk) const
{
    BulkVarSet& bvs = bk.vs;
    const USI& np = bvs.np;

    for (OCP_USI n = 0; n < bvs.nb; n++) {
        if (bvs.cType[n] == BulkContent::rf) {
            auto SAT = bk.SATm.GetSAT(n);

            OCP_USI bId = n * np;
            SAT->CalKrPcFIM(&bvs.S[bId], n);
            copy(SAT->GetKr().begin(), SAT->GetKr().end(), &bvs.kr[bId]);
            copy(SAT->GetPc().begin(), SAT->GetPc().end(), &bvs.Pc[bId]);
            copy(SAT->GetdKrdS().begin(), SAT->GetdKrdS().end(), &bvs.dKrdS[bId * np]);
            copy(SAT->GetdPcdS().begin(), SAT->GetdPcdS().end(), &bvs.dPcdS[bId * np]);
            for (USI j = 0; j < np; j++) bvs.Pj[n * np + j] = bvs.P[n] + bvs.Pc[n * np + j];
        }
    }
}

void T_FIM::CalThermalConduct(BulkConn& conn, Bulk& bk) const
{
    BulkVarSet& bvs = bk.vs;
    const OCP_USI nb = bvs.nb;
    const OCP_USI np = bvs.np;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bvs.cType[n] == BulkContent::rf) {
            // fluid bulk
            OCP_DBL tmp = 0;
            for (USI j = 0; j < np; j++) {
                tmp += bvs.S[n * np + j] * bk.thconp[j];
                bvs.ktS[n * np + j] = bvs.poro[n] * bk.thconp[j];
            }
            bvs.kt[n]  = bvs.poro[n] * tmp + (1 - bvs.poro[n]) * bvs.thconr[n];
            bvs.ktP[n] = bvs.poroP[n] * (tmp - bvs.thconr[n]);
            bvs.ktT[n] = bvs.poroT[n] * (tmp - bvs.thconr[n]);
        } else {
            // non fluid bulk
            bvs.kt[n] = bvs.thconr[n];
        }
    }
}


void T_FIM::ResetToLastTimeStep(Reservoir& rs, OCPControl& ctrl)
{
    // Bulk
    Bulk& bk = rs.bulk;
    BulkVarSet& bvs = bk.vs;
    // Rock
    bvs.poro   = bvs.lporo;
    bvs.rockVp = bvs.lrockVp;
    bvs.vr     = bvs.lvr;
    bvs.Hr     = bvs.lHr;
    // derivatives
    bvs.poroP = bvs.lporoP;
    bvs.poroT = bvs.lporoT;
    bvs.vrP   = bvs.lvrP;
    bvs.vrT   = bvs.lvrT;
    bvs.HrT   = bvs.lHrT;

    // Fluid
    bvs.phaseNum   = bvs.lphaseNum;
    bvs.Nt         = bvs.lNt;
    bvs.Ni         = bvs.lNi;
    bvs.vf         = bvs.lvf;
    bvs.T          = bvs.lT;
    bvs.P          = bvs.lP;
    bvs.Pj         = bvs.lPj;
    bvs.Pc         = bvs.lPc;
    bvs.phaseExist = bvs.lphaseExist;
    bvs.S          = bvs.lS;
    bvs.xij        = bvs.lxij;
    bvs.rho        = bvs.lrho;
    bvs.xi         = bvs.lxi;
    bvs.mu         = bvs.lmu;
    bvs.kr         = bvs.lkr;
    bvs.Uf         = bvs.lUf;
    bvs.H          = bvs.lH;
    bvs.kt         = bvs.lkt;
    // derivatives
    bvs.vfP       = bvs.lvfP;
    bvs.vfT       = bvs.lvfT;
    bvs.vfi       = bvs.lvfi;
    bvs.rhoP      = bvs.lrhoP;
    bvs.rhoT      = bvs.lrhoT;
    bvs.rhox      = bvs.lrhox;
    bvs.xiP       = bvs.lxiP;
    bvs.xiT       = bvs.lxiT;
    bvs.xix       = bvs.lxix;
    bvs.muP       = bvs.lmuP;
    bvs.muT       = bvs.lmuT;
    bvs.mux       = bvs.lmux;
    bvs.dPcdS   = bvs.ldPcdS;
    bvs.dKrdS    = bvs.ldKrdS;
    bvs.UfP       = bvs.lUfP;
    bvs.UfT       = bvs.lUfT;
    bvs.Ufi       = bvs.lUfi;
    bvs.HT        = bvs.lHT;
    bvs.Hx        = bvs.lHx;
    bvs.ktP       = bvs.lktP;
    bvs.ktT       = bvs.lktT;
    bvs.ktS       = bvs.lktS;
    bvs.dSec_dPri = bvs.ldSec_dPri;

    bk.BCm.ResetToLastTimeStep();

    // Wells
    rs.allWells.ResetBHP();
    rs.allWells.CalTrans(bk);
    rs.allWells.CaldG(bk);
    rs.allWells.CalFlux(bk);

    rs.optFeatures.ResetToLastTimeStep();

    // Iters
    ctrl.ResetIterNRLS();

    CalRes(rs, ctrl.GetCurTime() + ctrl.GetCurDt(), ctrl.GetCurDt(), OCP_TRUE);
}

void T_FIM::UpdateLastTimeStep(Reservoir& rs) const
{
    // Bulk
    Bulk& bk = rs.bulk;
    BulkVarSet& bvs = bk.vs;
    // Rock
    bvs.lporo   = bvs.poro;
    bvs.lrockVp = bvs.rockVp;
    bvs.lvr     = bvs.vr;
    bvs.lHr     = bvs.Hr;
    // derivatives
    bvs.lporoP = bvs.poroP;
    bvs.lporoT = bvs.poroT;
    bvs.lvrP   = bvs.vrP;
    bvs.lvrT   = bvs.vrT;
    bvs.lHrT   = bvs.HrT;

    // Fluid
    bvs.lphaseNum   = bvs.phaseNum;
    bvs.lNt         = bvs.Nt;
    bvs.lNi         = bvs.Ni;
    bvs.lvf         = bvs.vf;
    bvs.lT          = bvs.T;
    bvs.lP          = bvs.P;
    bvs.lPj         = bvs.Pj;
    bvs.lPc         = bvs.Pc;
    bvs.lphaseExist = bvs.phaseExist;
    bvs.lS          = bvs.S;
    bvs.lxij        = bvs.xij;
    bvs.lrho        = bvs.rho;
    bvs.lxi         = bvs.xi;
    bvs.lmu         = bvs.mu;
    bvs.lkr         = bvs.kr;
    bvs.lUf         = bvs.Uf;
    bvs.lH          = bvs.H;
    bvs.lkt         = bvs.kt;
    // derivatives
    bvs.lvfP       = bvs.vfP;
    bvs.lvfT       = bvs.vfT;
    bvs.lvfi       = bvs.vfi;
    bvs.lrhoP      = bvs.rhoP;
    bvs.lrhoT      = bvs.rhoT;
    bvs.lrhox      = bvs.rhox;
    bvs.lxiP       = bvs.xiP;
    bvs.lxiT       = bvs.xiT;
    bvs.lxix       = bvs.xix;
    bvs.lmuP       = bvs.muP;
    bvs.lmuT       = bvs.muT;
    bvs.lmux       = bvs.mux;
    bvs.ldPcdS   = bvs.dPcdS;
    bvs.ldKrdS    = bvs.dKrdS;
    bvs.lUfP       = bvs.UfP;
    bvs.lUfT       = bvs.UfT;
    bvs.lUfi       = bvs.Ufi;
    bvs.lHT        = bvs.HT;
    bvs.lHx        = bvs.Hx;
    bvs.lktP       = bvs.ktP;
    bvs.lktT       = bvs.ktT;
    bvs.lktS       = bvs.ktS;
    bvs.ldSec_dPri = bvs.dSec_dPri;

    bk.BCm.UpdateLastTimeStep();

    rs.allWells.UpdateLastTimeStepBHP();
    rs.optFeatures.UpdateLastTimeStep();
}

void T_FIM::CalRes(Reservoir&      rs,
                   const OCP_DBL&  t,
                   const OCP_DBL&  dt,
                   const OCP_BOOL& resetRes0)
{
    const Bulk& bk   = rs.bulk;
    const BulkVarSet& bvs = bk.vs;
    const USI   nb   = bvs.nbI;
    const USI   np   = bvs.np;
    const USI   nc   = bvs.nc;
    const USI   len  = nc + 2;
    
    res.SetZero();

    // Bulk to Bulk
    OCP_USI bId, eId, bIdb;
    // Accumalation Term
    for (OCP_USI n = 0; n < nb; n++) {
        if (bvs.cType[n] == BulkContent::rf) {
            // Fluid bulk
            bId  = n * len;
            bIdb = n * nc;
            // Volume Conservation
            res.resAbs[bId] = bvs.rockVp[n] - bvs.vf[n];
            // Mass Conservation
            for (USI i = 0; i < nc; i++) {
                res.resAbs[n * len + 1 + i] = bvs.Ni[bIdb + i] - bvs.lNi[bIdb + i];
            }
            // Energy Conservation
            res.resAbs[n * len + nc + 1] =
                (bvs.vf[n] * bvs.Uf[n] + bvs.vr[n] * bvs.Hr[n]) -
                (bvs.lvf[n] * bvs.lUf[n] + bvs.lvr[n] * bvs.lHr[n]);
        } else {
            // Non fluid bulk
            res.resAbs[n * len + nc + 1] = bvs.vr[n] * bvs.Hr[n] - bvs.lvr[n] * bvs.lHr[n];
        }

        // Heat Loss
        if (OCP_TRUE) {
            // dT
            res.resAbs[n * len + nc + 1] += dt * bk.BCm.heatLoss.GetHl(n);
        }
    }

    BulkConn&         conn = rs.conn;
    BulkConnVarSet&   bcvs = conn.vs;
    USI     fluxnum;

    for (OCP_USI c = 0; c < conn.numConn; c++) {
        bId       = conn.iteratorConn[c].BId();
        eId       = conn.iteratorConn[c].EId();
        fluxnum   = conn.iteratorConn[c].FluxNum();
        auto Flux = conn.flux[fluxnum];

        Flux->CalFlux(conn.iteratorConn[c], bk);

        // Thermal conductive term always exists
        const auto conH = Flux->GetConductH();
        res.resAbs[bId * len + 1 + nc] += conH * dt;
        if (eId < nb) {
            // Interior grid
            res.resAbs[eId * len + 1 + nc] -= conH * dt;
        }

        if (bvs.cType[bId] == BulkContent::rf && bvs.cType[eId] == BulkContent::rf) {
            // with fluid flow
            copy(Flux->GetUpblock().begin(), Flux->GetUpblock().end(), &bcvs.upblock[c * np]);
            copy(Flux->GetRho().begin(), Flux->GetRho().end(), &bcvs.rho[c * np]);
            copy(Flux->GetFluxVj().begin(), Flux->GetFluxVj().end(), &bcvs.flux_vj[c * np]);

			if (eId < nb) {
				// Interior grid
				for (USI i = 0; i < nc; i++) {
					res.resAbs[bId * len + 1 + i] += dt * Flux->GetFluxNi()[i];
					res.resAbs[eId * len + 1 + i] -= dt * Flux->GetFluxNi()[i];
				}
                for (USI j = 0; j < np; j++) {
                    res.resAbs[bId * len + 1 + nc] += dt * Flux->GetFluxHj()[j];
                    res.resAbs[eId * len + 1 + nc] -= dt * Flux->GetFluxHj()[j];
                }
			}
			else {
				// Ghost grid
				for (USI i = 0; i < nc; i++) {
					res.resAbs[bId * len + 1 + i] += dt * Flux->GetFluxNi()[i];
				}
                for (USI j = 0; j < np; j++) {
                    res.resAbs[bId * len + 1 + nc] += dt * Flux->GetFluxHj()[j];
                }				
			}
        }
    }

    // Well to Bulk
    USI wId = nb * len;
    for (const auto& wl : rs.allWells.wells) {
        wl.CalResFIM_T(wId, res, bk, dt);
    }

    // Calculate RelRes
    OCP_DBL tmp;
    for (OCP_USI n = 0; n < nb; n++) {

        // Energy equations always exist
        OCP_DBL eT = bvs.vf[n] * bvs.Uf[n] + bvs.vr[n] * bvs.Hr[n];
        tmp        = fabs(res.resAbs[n * len + nc + 1] / eT);
        if (res.maxRelRes_E < tmp) {
            res.maxRelRes_E = tmp;
            res.maxId_E     = n;
        }

        if (bvs.cType[n] == BulkContent::rf) {
            // Fluid Bulk
            for (USI i = 0; i < len - 1; i++) {
                tmp = fabs(res.resAbs[n * len + i] / bvs.rockVp[n]);
                if (res.maxRelRes_V < tmp) {
                    res.maxRelRes_V = tmp;
                    res.maxId_V     = n;
                }
            }

            for (USI i = 1; i < len - 1; i++) {
                tmp = fabs(res.resAbs[n * len + i] / bvs.Nt[n]);
                if (res.maxRelRes_N < tmp) {
                    res.maxRelRes_N = tmp;
                    res.maxId_N     = n;
                }
            }
        }
    }

    Dscalar(res.resAbs.size(), -1.0, res.resAbs.data());
    if (resetRes0) {
        res.SetInitRes();

        GetWallTime timer;
        timer.Start();

        OCP_DBL tmploc = res.maxRelRes0_V;
        MPI_Allreduce(&tmploc, &res.maxRelRes0_V, 1, MPI_DOUBLE, MPI_MIN, rs.domain.myComm);

        OCPTIME_COMM_COLLECTIVE += timer.Stop() / 1000;
    }
}

void T_FIM::AssembleMatBulks(LinearSystem&    ls,
                             const Reservoir& rs,
                             const OCP_DBL&   t,
                             const OCP_DBL&   dt) const
{
    const USI numWell = rs.GetNumOpenWell();

    const Bulk&     bk     = rs.bulk;
    const BulkVarSet& bvs = bk.vs;
    const BulkConn& conn   = rs.conn;
    const OCP_USI   nb     = bvs.nbI;
    const USI       np     = bvs.np;
    const USI       nc     = bvs.nc;
    const USI       ncol   = nc + 2;
    const USI       ncol2  = np * nc + np;
    const USI       bsize  = ncol * ncol;
    const USI       bsize2 = ncol * ncol2;

    ls.AddDim(nb);

    vector<OCP_DBL> bmat(bsize, 0);
    // Accumulation term
    for (USI i = 1; i < nc + 1; i++) {
        // Mass consevation
        bmat[i * ncol + i] = 1;
    }
    vector<OCP_DBL> bmatR(bmat);
    bmatR[0] = 1;
    for (OCP_USI n = 0; n < nb; n++) {
        if (bvs.cType[n] == BulkContent::rf) {
            // Fluid Bulk
            // Volume consevation
            // dP
            bmat[0] = bvs.v[n] * bvs.poroP[n] - bvs.vfP[n];
            // dNi
            for (USI i = 0; i < nc; i++) {
                bmat[i + 1] = -bvs.vfi[n * nc + i];
            }
            // dT
            bmat[nc + 1] = bvs.v[n] * bvs.poroT[n] - bvs.vfT[n];
            // Energy consevation
            // dP
            bmat[ncol * (ncol - 1)] =
                bvs.vfP[n] * bvs.Uf[n] + bvs.vf[n] * bvs.UfP[n] + bvs.vrP[n] * bvs.Hr[n];
            // dNi
            for (USI i = 0; i < nc; i++) {
                bmat[ncol * (ncol - 1) + i + 1] =
                    bvs.vfi[n * nc + i] * bvs.Uf[n] + bvs.vf[n] * bvs.Ufi[n * nc + i];
            }
            // dT
            bmat[ncol * ncol - 1] = bvs.vfT[n] * bvs.Uf[n] + bvs.vf[n] * bvs.UfT[n] +
                                    bvs.vrT[n] * bvs.Hr[n] + bvs.vr[n] * bvs.HrT[n];

            // Heat Loss iterm
            if (OCP_TRUE) {
                // dT
                bmat[ncol * ncol - 1] += dt * bk.BCm.heatLoss.GetHlT(n);
            }

            ls.NewDiag(n, bmat);
        } else {
            // Non Fluid Bulk
            // Energy consevation
            // dT
            bmatR[ncol * ncol - 1] = bvs.vrT[n] * bvs.Hr[n] + bvs.vr[n] * bvs.HrT[n];

            // Heat Loss iterm
            if (OCP_TRUE) {
                // dT
                bmatR[ncol * ncol - 1] += dt * bk.BCm.heatLoss.GetHlT(n);
            }

            ls.NewDiag(n, bmatR);
        }
    }

    // flux term
    OCP_USI  bId, eId;
    USI      fluxnum;
    for (OCP_USI c = 0; c < conn.numConn; c++) {

        bId       = conn.iteratorConn[c].BId();
        eId       = conn.iteratorConn[c].EId();
        fluxnum   = conn.iteratorConn[c].FluxNum();
        auto Flux = conn.flux[fluxnum];

        Flux->AssembleMatFIM(conn.iteratorConn[c], c, conn.vs, bk);

        bmat = Flux->GetdFdXpB();
        DaABpbC(ncol, ncol, ncol2, 1, Flux->GetdFdXsB().data(), &bvs.dSec_dPri[bId * bsize2], 1,
            bmat.data());

        Dscalar(bsize, dt, bmat.data());
        // Begin - Begin -- add
        ls.AddDiag(bId, bmat);
        // End - Begin -- insert
        if (eId < nb) {
            // Interior grid
            Dscalar(bsize, -1, bmat.data());
            ls.NewOffDiag(eId, bId, bmat);
        }
        

#ifdef OCP_NANCHECK
        if (!CheckNan(bmat.size(), &bmat[0])) {
            OCP_ABORT("INF or INF in bmat !");
        }
#endif

        bmat = Flux->GetdFdXpE();
        DaABpbC(ncol, ncol, ncol2, 1, Flux->GetdFdXsE().data(), &bvs.dSec_dPri[eId * bsize2], 1,
            bmat.data());

        Dscalar(bsize, dt, bmat.data());

        if (eId < nb) {
            // Interior grid
            // Begin - End -- insert
            ls.NewOffDiag(bId, eId, bmat);
            // End - End -- add
            Dscalar(bsize, -1, bmat.data());
            ls.AddDiag(eId, bmat);
        }
        else {
            // Ghost grid
            // Begin - End -- insert
            ls.NewOffDiag(bId, eId + numWell, bmat);
        }

#ifdef OCP_NANCHECK
        if (!CheckNan(bmat.size(), &bmat[0])) {
            OCP_ABORT("INF or INF in bmat !");
        }
#endif
    }
}

void T_FIM::AssembleMatWells(LinearSystem&    ls,
                             const Reservoir& rs,
                             const OCP_DBL&   dt) const
{
    for (auto& wl : rs.allWells.wells) {
        wl.AssembleMatFIM_T(ls, rs.bulk, dt);
    }
}


void T_FIM::GetSolution(Reservoir&             rs,
                        vector<OCP_DBL>& u,
                        const OCPControl&      ctrl)
{

    const Domain&   domain = rs.domain;
    Bulk&           bk     = rs.bulk;
    BulkVarSet& bvs = bk.vs;
    const OCP_USI   nb     = bvs.nb;
    const USI       np     = bvs.np;
    const USI       nc     = bvs.nc;
    const USI       row    = np * (nc + 1);
    const USI       col    = nc + 2;

    // Well first
    USI wId = bvs.nbI * col;
    for (auto& wl : rs.allWells.wells) {
        if (wl.IsOpen()) {
            wl.SetBHP(wl.BHP() + u[wId]);
            wId += col;
        }
    }

    // Exchange Solution for ghost grid
    for (USI i = 0; i < domain.numRecvProc; i++) {
        const vector<OCP_USI>& rel = domain.recv_element_loc[i];
        MPI_Irecv(&u[rel[1] * col], (rel[2] - rel[1]) * col, MPI_DOUBLE, rel[0], 0, domain.myComm, &domain.recv_request[i]);
    }

    vector<vector<OCP_DBL>> send_buffer(domain.numSendProc);
    for (USI i = 0; i < domain.numSendProc; i++) {
        const vector<OCP_USI>& sel = domain.send_element_loc[i];
        vector<OCP_DBL>&       s   = send_buffer[i];
        s.resize(1 + (sel.size() - 1) * col);
        s[0] = sel[0];
        for (USI j = 1; j < sel.size(); j++) {
            const OCP_DBL* bId = u.data() + sel[j] * col;
            copy(bId, bId + col, &s[1 + (j - 1) * col]);
        }
        MPI_Isend(s.data() + 1, s.size() - 1, MPI_DOUBLE, s[0], 0, domain.myComm, &domain.send_request[i]);
    }

    // Bulk
    const OCP_DBL dSmaxlim = ctrl.ctrlNR.NRdSmax;
    // const OCP_DBL dPmaxlim = ctrl.ctrlNR.NRdPmax;

    vector<OCP_DBL> dtmp(row, 0);
    OCP_DBL         chopmin = 1;
    OCP_DBL         choptmp = 0;

    dSNR    = bvs.S;
    NRdPmax = 0;
    NRdNmax = 0;
    NRdTmax = 0;

    OCP_USI bId = 0;
    OCP_USI eId = bk.GetInteriorBulkNum();

    // interior first, ghost second
    for (USI p = bId; p < eId; p++) {

        for (OCP_USI n = bId; n < eId; n++) {
            // const vector<OCP_DBL>& scm = satcm[SATNUM[n]];

            if (bvs.cType[n] == BulkContent::rf) {
                // Fluid Bulk

                chopmin = 1;
                // compute the chop
                fill(dtmp.begin(), dtmp.end(), 0.0);
                DaAxpby(np, col, 1, &bvs.dSec_dPri[n * bvs.lendSdP], u.data() + n * col, 1,
                    dtmp.data());

                for (USI j = 0; j < np; j++) {
                    choptmp = 1;
                    if (fabs(dtmp[j]) > dSmaxlim) {
                        choptmp = dSmaxlim / fabs(dtmp[j]);
                    }
                    else if (bvs.S[n * np + j] + dtmp[j] < 0.0) {
                        choptmp = 0.9 * bvs.S[n * np + j] / fabs(dtmp[j]);
                    }
                    chopmin = min(chopmin, choptmp);
                }

                // dS
                for (USI j = 0; j < np; j++) {
                    bvs.S[n * np + j] += chopmin * dtmp[j];
                }

                // dP
                OCP_DBL dP = u[n * col];
                if (fabs(NRdPmax) < fabs(dP)) NRdPmax = dP;
                bvs.P[n] += dP;
                dPNR[n] = dP;

                // dNi
                for (USI i = 0; i < nc; i++) {
                    dNNR[n * nc + i] = u[n * col + 1 + i] * chopmin;
                    if (fabs(NRdNmax) < fabs(dNNR[n * nc + i]) / bvs.Nt[n])
                        NRdNmax = dNNR[n * nc + i] / bvs.Nt[n];

                    bvs.Ni[n * nc + i] += dNNR[n * nc + i];
                }
            }

            // dT
            OCP_DBL dT = u[n * col + col - 1];
            if (fabs(NRdTmax) < fabs(dT)) NRdTmax = dT;
            bvs.T[n] += dT;
            dTNR[n] = dT;
        }

        if (p == 0) {
            bId = eId;
            eId = nb;
            MPI_Waitall(domain.numRecvProc, domain.recv_request.data(), MPI_STATUS_IGNORE);
        }
        else {
            break;
        }
    }

    MPI_Waitall(domain.numSendProc, domain.send_request.data(), MPI_STATUS_IGNORE);
}

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Nov/10/2022      Create file                          */
/*----------------------------------------------------------------------------*/
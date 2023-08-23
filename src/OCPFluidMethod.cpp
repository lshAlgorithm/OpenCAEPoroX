/*! \file    OCPFluidMethod.cpp
 *  \brief   Definition of solution methods for fluid part in OpenCAEPoroX
 *  \author  Shizhe Li
 *  \date    Nov/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "OCPFluidMethod.hpp"

////////////////////////////////////////////
// IsothermalMethod
////////////////////////////////////////////


void IsothermalMethod::CalRock(Bulk& bk) const
{
    auto& bvs = bk.vs;
    for (OCP_USI n = 0; n < bvs.nb; n++) {
        auto ROCK = bk.ROCKm.GetROCK(n);

        ROCK->CalPoro(bvs.P[n], bvs.T[n], bvs.poroInit[n], 0);
        bvs.poro[n]   = ROCK->GetPoro();
        bvs.poroP[n]  = ROCK->GetdPorodP();
        bvs.rockVp[n] = bvs.v[n] * bvs.poro[n];
    }
}

////////////////////////////////////////////
// IsoT_IMPEC
////////////////////////////////////////////

void IsoT_IMPEC::Setup(Reservoir& rs, LinearSystem& ls, const OCPControl& ctrl)
{
    // Allocate Memory of auxiliary variables for IMPEC
    AllocateReservoir(rs);
    // Allocate Memory of Matrix for IMPEC
    AllocateLinearSystem(ls, rs, ctrl);
}

/// Initialize reservoir
void IsoT_IMPEC::InitReservoir(Reservoir& rs) const
{
    rs.bulk.InitPTSw(50);

    CalRock(rs.bulk);

    InitFlash(rs.bulk);
    CalKrPc(rs.bulk);

    CalBulkFlux(rs);

    rs.allWells.InitBHP(rs.bulk);

    UpdateLastTimeStep(rs);
}

void IsoT_IMPEC::Prepare(Reservoir& rs, OCPControl& ctrl)
{
    rs.allWells.PrepareWell(rs.bulk);
    rs.CalCFL(ctrl.GetCurDt(), OCP_TRUE);
    ctrl.Check(rs, {"CFL"});
}

void IsoT_IMPEC::AssembleMat(LinearSystem&    ls,
                             const Reservoir& rs,
                             const OCP_DBL&   dt) const
{
    AssembleMatBulks(ls, rs, dt);
    AssembleMatWells(ls, rs, dt);
}

void IsoT_IMPEC::SolveLinearSystem(LinearSystem& ls, Reservoir& rs, OCPControl& ctrl)
{
#ifdef DEBUG
    ls.CheckEquation();
#endif // DEBUG

    GetWallTime timer;

    timer.Start();
    ls.CalCommTerm(rs.GetNumOpenWell());
    ls.AssembleMatLinearSolver();
    OCPTIME_ASSEMBLE_MAT_FOR_LS += timer.Stop() / 1000;
    
    timer.Start();
    int status = ls.Solve();
    if (status < 0) {
        status = ls.GetNumIters();
    }

#ifdef DEBUG
    //OCP_INT myrank = rs.domain.myrank;
    //ls.OutputLinearSystem("proc" + to_string(myrank) + "_testA_IMPEC.out", 
    //                      "proc" + to_string(myrank) + "_testb_IMPEC.out");
    //ls.OutputSolution("proc" + to_string(myrank) + "_testx_IMPEC.out");
    //MPI_Barrier(rs.domain.myComm);
    //OCP_ABORT("Stop");
#endif // DEBUG

    OCPTIME_LSOLVER += timer.Stop() / 1000;
    ctrl.UpdateIterLS(status);
    ctrl.UpdateIterNR();

#ifdef DEBUG
    // ls.OutputSolution("testx_IMPEC.out");
#endif // DEBUG

    timer.Start();
    GetSolution(rs, ls.GetSolution());
    OCPTIME_NRSTEP += timer.Stop() / 1000;
    ls.ClearData();
}

OCP_BOOL IsoT_IMPEC::UpdateProperty(Reservoir& rs, OCPControl& ctrl)
{
    OCP_DBL& dt = ctrl.current_dt;

    // First check : Pressure check
    if (!ctrl.Check(rs, {"BulkP", "WellP"})) {
        return OCP_FALSE;
    }

    // Calculate Flux between bulks and between bulks and wells
    CalFlux(rs);  
    MassConserve(rs, dt);

    // Second check : CFL check
    rs.CalCFL(dt, OCP_TRUE);
    // Third check: Ni check
    if (!ctrl.Check(rs, { "CFL","BulkNi"})) {
        ResetToLastTimeStep01(rs, ctrl);
        return OCP_FALSE;
    }

    CalRock(rs.bulk);
    CalFlash(rs.bulk);

    // Fouth check: Volume error check
    if (!ctrl.Check(rs, {"BulkVe"})) {
        ResetToLastTimeStep02(rs, ctrl);
        return OCP_FALSE;
    }

    CalKrPc(rs.bulk);
    CalBulkFlux(rs);

    return OCP_TRUE;
}

OCP_BOOL IsoT_IMPEC::FinishNR(const Reservoir& rs) { return OCP_TRUE; }

void IsoT_IMPEC::FinishStep(Reservoir& rs, OCPControl& ctrl)
{
    rs.CalIPRT(ctrl.GetCurDt());
    rs.CalMaxChange();
    UpdateLastTimeStep(rs);
    // ctrl.CalNextTstepIMPEC(rs);
    ctrl.CalNextTimeStep(rs, {"dP", "dN", "dS", "eV"});
}

void IsoT_IMPEC::AllocateReservoir(Reservoir& rs)
{
    Bulk&         bk  = rs.bulk;
    BulkVarSet&   bvs = bk.vs;
    const OCP_USI nb  = bvs.nb;
    const USI     np  = bvs.np;
    const USI     nc  = bvs.nc;

    // Rock
    bvs.poro.resize(nb);
    bvs.rockVp.resize(nb);

    bvs.lporo.resize(nb);
    bvs.lrockVp.resize(nb);

    // derivatives
    bvs.poroP.resize(nb);
    bvs.lporoP.resize(nb);

    // Fluid
    bvs.phaseNum.resize(nb);
    bvs.Nt.resize(nb);
    bvs.Ni.resize(nb * nc);
    bvs.vf.resize(nb);
    bvs.T.resize(nb);
    bvs.P.resize(nb);
    bvs.Pb.resize(nb);
    bvs.Pj.resize(nb * np);
    bvs.Pc.resize(nb * np);
    bvs.phaseExist.resize(nb * np);
    bvs.S.resize(nb * np);
    bvs.vj.resize(nb * np);
    bvs.xij.resize(nb * np * nc);
    bvs.rho.resize(nb * np);
    bvs.xi.resize(nb * np);
    bvs.mu.resize(nb * np);
    bvs.kr.resize(nb * np);

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
    bvs.vj.resize(nb * np);
    bvs.lxij.resize(nb * np * nc);
    bvs.lrho.resize(nb * np);
    bvs.lxi.resize(nb * np);
    bvs.lmu.resize(nb * np);
    bvs.lkr.resize(nb * np);

    // derivatives
    bvs.vfP.resize(nb);
    bvs.vfi.resize(nb * nc);

    bvs.lvfP.resize(nb);
    bvs.lvfi.resize(nb * nc);

    // others
    bk.cfl.resize(nb * np);

    BulkConn& conn = rs.conn;

    conn.bcval.upblock.resize(conn.numConn * np);
    conn.bcval.rho.resize(conn.numConn * np);
    conn.bcval.velocity.resize(conn.numConn * np);
    conn.bcval.flux_ni.resize(conn.numConn * nc);

    conn.bcval.lupblock.resize(conn.numConn * np);
    conn.bcval.lrho.resize(conn.numConn * np);
}

void IsoT_IMPEC::AllocateLinearSystem(LinearSystem&     ls,
                                      const Reservoir&  rs,
                                      const OCPControl& ctrl)
{
    ls.SetupDomain(rs.domain);
    ls.AllocateRowMem(1);
    ls.AllocateColMem();
    ls.SetupLinearSolver(ISOTHERMALMODEL, ctrl.GetWorkDir(), ctrl.GetLsFile());
}

void IsoT_IMPEC::InitFlash(Bulk& bk) const
{
    BulkVarSet& bvs = bk.vs;

    for (OCP_USI n = 0; n < bvs.nb; n++) {

        auto PVT = bk.PVTm.GetPVT(n);
        PVT->InitFlashIMPEC(bvs.P[n], bvs.Pb[n], bvs.T[n], &bvs.S[n * bvs.np],
                                      bvs.rockVp[n], bvs.Ni.data() + n * bvs.nc, n);
        
        for (USI i = 0; i < bvs.nc; i++) {
            bvs.Ni[n * bvs.nc + i] = PVT->GetNi(i);
        }
        PassFlashValue(bk, n);
    }
}

void IsoT_IMPEC::CalFlash(Bulk& bk)
{
    const BulkVarSet& bvs = bk.vs;

    for (OCP_USI n = 0; n < bvs.nb; n++) {

        bk.PVTm.GetPVT(n)->FlashIMPEC(bvs.P[n], bvs.T[n], &bvs.Ni[n * bvs.nc],
                                  bvs.phaseNum[n], &bvs.xij[n * bvs.np * bvs.nc], n);
        PassFlashValue(bk, n);
    }
}

void IsoT_IMPEC::PassFlashValue(Bulk& bk, const OCP_USI& n) const
{
    BulkVarSet& bvs = bk.vs;
    const auto  PVT = bk.PVTm.GetPVT(n);

    const USI     np     = bvs.np;
    const USI     nc     = bvs.nc;
    const OCP_USI bIdp   = n * np;

    bvs.phaseNum[n] = 0;
    bvs.Nt[n]       = PVT->GetNt();
    bvs.vf[n]       = PVT->GetVf();

    for (USI j = 0; j < np; j++) {
        // Important! Saturation must be passed no matter if the phase exists. This is
        // because it will be used to calculate relative permeability and capillary
        // pressure at each time step. Make sure that all saturations are updated at
        // each step!
        bvs.phaseExist[bIdp + j] = PVT->GetPhaseExist(j);
        bvs.S[bIdp + j]          = PVT->GetS(j);
        if (bvs.phaseExist[bIdp + j]) {
            bvs.phaseNum[n]++;
            for (USI i = 0; i < nc; i++) {
                bvs.xij[bIdp * nc + j * nc + i] = PVT->GetXij(j, i);
            }
            bvs.vj[bIdp + j]  = PVT->GetVj(j);
            bvs.rho[bIdp + j] = PVT->GetRho(j);
            bvs.xi[bIdp + j]  = PVT->GetXi(j);
            bvs.mu[bIdp + j]  = PVT->GetMu(j);
        }
    }

    bvs.vfP[n] = PVT->GetVfP();
    for (USI i = 0; i < nc; i++) {
        bvs.vfi[n * nc + i] = PVT->GetVfi(i);
    }
}

void IsoT_IMPEC::CalKrPc(Bulk& bk) const
{
    BulkVarSet& bvs = bk.vs;

    for (OCP_USI n = 0; n < bvs.nb; n++) {

        auto SAT = bk.SATm.GetSAT(n);

        OCP_USI bId = n * bvs.np;
        SAT->CalKrPc(&bvs.S[bId], n);
        copy(SAT->GetKr().begin(), SAT->GetKr().end(), &bvs.kr[bId]);
        copy(SAT->GetPc().begin(), SAT->GetPc().end(), &bvs.Pc[bId]);
        for (USI j = 0; j < bvs.np; j++)
            bvs.Pj[n * bvs.np + j] = bvs.P[n] + bvs.Pc[n * bvs.np + j];
    }
}

void IsoT_IMPEC::CalFlux(Reservoir& rs) const
{
    CalBulkFlux(rs);
    rs.allWells.CalFlux(rs.bulk);
}

void IsoT_IMPEC::CalBulkFlux(Reservoir& rs) const
{
    const Bulk&       bk  = rs.bulk;
    const BulkVarSet& bvs = bk.vs;

    BulkConn&   conn = rs.conn;
    const USI   np   = bvs.np;
    const USI   nc   = bvs.nc;

    // calculate a step flux using iteratorConn
    BulkConnValSet&   bcvs = conn.bcval;

    for (OCP_USI c = 0; c < conn.numConn; c++) {

        const auto cType = conn.iteratorConn[c].Type();
        auto       Flux  = conn.flux[cType];

        Flux->CalFlux(conn.iteratorConn[c], bk);
        copy(Flux->GetUpblock().begin(), Flux->GetUpblock().end(), &bcvs.upblock[c * np]);
        copy(Flux->GetRho().begin(), Flux->GetRho().end(), &bcvs.rho[c * np]);
        copy(Flux->GetFluxVj().begin(), Flux->GetFluxVj().end(), &bcvs.velocity[c * np]);
        copy(Flux->GetFluxNi().begin(), Flux->GetFluxNi().end(), &bcvs.flux_ni[c * nc]);
    }
}

void IsoT_IMPEC::MassConserve(Reservoir& rs, const OCP_DBL& dt) const
{

    BulkVarSet& bvs = rs.bulk.vs;

    // Bulk to Bulk
    const USI       nc   = bvs.nc;
    const BulkConn& conn = rs.conn;
    
    OCP_USI bId, eId;

    for (OCP_USI c = 0; c < conn.numConn; c++) {
        bId = conn.iteratorConn[c].BId();
        eId = conn.iteratorConn[c].EId();

        for (USI i = 0; i < nc; i++) {
            bvs.Ni[eId * nc + i] += dt * conn.bcval.flux_ni[c * nc + i];
            bvs.Ni[bId * nc + i] -= dt * conn.bcval.flux_ni[c * nc + i];
        }
    }

    // Well to Bulk
    for (auto& wl : rs.allWells.wells) {
        if (wl.IsOpen()) {
            for (USI p = 0; p < wl.PerfNum(); p++) {
                OCP_USI k = wl.PerfLocation(p);
                for (USI i = 0; i < nc; i++) {
                    bvs.Ni[k * nc + i] -= wl.PerfQi_lbmol(p, i) * dt;
                }
            }
        }
    }

    // Exchange Ghost Ni
    const Domain& domain = rs.domain;

    for (USI i = 0; i < domain.numRecvProc; i++) {
        const vector<OCP_USI>& rel = domain.recv_element_loc[i];
        MPI_Irecv(&bvs.Ni[rel[1] * nc], (rel[2] - rel[1]) * nc, MPI_DOUBLE, rel[0], 0, domain.myComm, &domain.recv_request[i]);
    }

    vector<vector<OCP_DBL>> send_buffer(domain.numSendProc);
    for (USI i = 0; i < domain.numSendProc; i++) {
        const vector<OCP_USI>& sel = domain.send_element_loc[i];
        vector<OCP_DBL>&       s   = send_buffer[i];
        s.resize(1 + (sel.size() - 1) * nc);
        s[0] = sel[0];
        for (USI j = 1; j < sel.size(); j++) {
            const OCP_DBL* bId = &bvs.Ni[0] + sel[j] * nc;
            copy(bId, bId + nc, &s[1 + (j - 1) * nc]);
        }
        MPI_Isend(s.data() + 1, s.size() - 1, MPI_DOUBLE, s[0], 0, domain.myComm, &domain.send_request[i]);
    }

    MPI_Waitall(domain.numSendProc, domain.send_request.data(), MPI_STATUS_IGNORE);
    MPI_Waitall(domain.numRecvProc, domain.recv_request.data(), MPI_STATUS_IGNORE);
}

void IsoT_IMPEC::AssembleMatBulks(LinearSystem&    ls,
                                  const Reservoir& rs,
                                  const OCP_DBL&   dt) const
{
    const Bulk&       bk  = rs.bulk;
    const BulkVarSet& bvs = bk.vs;

    const USI numWell = rs.GetNumOpenWell();
    const BulkConn& conn = rs.conn;
    const OCP_USI   nb   = bvs.nbI;

    ls.AddDim(nb);

    // accumulate term
    OCP_DBL Vpp, Vp, vf, vfP, P;
    for (OCP_USI n = 0; n < nb; n++) {
        vf  = bvs.vf[n];
        vfP = bvs.vfP[n];
        P   = bvs.lP[n];
        Vpp = bvs.v[n] * bvs.poroP[n];
        Vp  = bvs.rockVp[n];

        ls.NewDiag(n, Vpp - vfP);
        ls.AddRhs(n, (Vpp - vfP) * P + dt * (vf - Vp));
    }


    // flux term
    OCP_USI bId, eId;
    USI     cType;
    OCP_DBL valbb, rhsb, valee, rhse;

    // Be careful when first bulk has no neighbors!
    for (OCP_USI c = 0; c < conn.numConn; c++) {
        bId   = conn.iteratorConn[c].BId();
        eId   = conn.iteratorConn[c].EId();
        cType = conn.iteratorConn[c].Type();
        auto Flux = conn.flux[cType];

        Flux->AssembleMatIMPEC(conn.iteratorConn[c], c, conn.bcval, bk);
        valbb  = dt * Flux->GetValbb();
        valee  = dt * Flux->GetValee();
        rhsb   = dt * Flux->GetRhsb();
        rhse   = dt * Flux->GetRhse();


        if (eId < nb) {
            // interior grid
            ls.AddDiag(bId, valbb);
            ls.AddDiag(eId, valee);
            ls.NewOffDiag(bId, eId, -valbb);
            ls.NewOffDiag(eId, bId, -valee);
            ls.AddRhs(bId, rhsb);
            ls.AddRhs(eId, rhse);
        }
        else {
            // ghost grid
            ls.AddDiag(bId, valbb);
            ls.NewOffDiag(bId, eId + numWell, -valbb);
            ls.AddRhs(bId, rhsb);
        }
    }
}

void IsoT_IMPEC::AssembleMatWells(LinearSystem&    ls,
                                  const Reservoir& rs,
                                  const OCP_DBL&   dt) const
{
    for (auto& wl : rs.allWells.wells) {
        wl.AssembleMatIMPEC(ls, rs.bulk, dt);
    }

    // for Reinjection
    // for (auto& wG : wellGroup) {
    //    if (wG.reInj) {
    //        for (auto& prod : wellGroup[wG.prodGroup].wIdPROD) {
    //            if (wells[prod].IsOpen()) {
    //                wells[prod].AssembleMatReinjection_IMPEC(myBulk, myLS, dt, wells,
    //                    wG.wIdINJ);
    //            }
    //        }
    //    }
    //}
}


void IsoT_IMPEC::GetSolution(Reservoir& rs, vector<OCP_DBL>& u)
{
    Bulk&       bk = rs.bulk;
    BulkVarSet& bvs = bk.vs;
    const OCP_USI nb     = bvs.nb;
    const USI     np     = bvs.np;
    const Domain& domain = rs.domain;

    // Well first
    USI wId = bvs.nbI;
    for (auto& wl : rs.allWells.wells) {
        if (wl.IsOpen()) {
            wl.SetBHP(u[wId]);
            wl.CalPerfP();
            wId++;
        }
    }

    // Exchange Solution

    for (USI i = 0; i < domain.numRecvProc; i++) {
        const vector<OCP_USI>& rel = domain.recv_element_loc[i];
        MPI_Irecv(&u[rel[1]], rel[2] - rel[1], MPI_DOUBLE, rel[0], 0, domain.myComm, &domain.recv_request[i]);
    }

    vector<vector<OCP_DBL>> send_buffer(domain.numSendProc);
    for (USI i = 0; i < domain.numSendProc; i++) {
        const vector<OCP_USI>& sel = domain.send_element_loc[i];
        vector<OCP_DBL>&       s   = send_buffer[i];
        s.resize(sel.size());
        s[0] = sel[0];
        for (USI j = 1; j < sel.size(); j++) {
            s[j] = u[sel[j]];
        }
        MPI_Isend(s.data() + 1, s.size() - 1, MPI_DOUBLE, s[0], 0, domain.myComm, &domain.send_request[i]);
    }

    // Bulk
    // interior first, ghost second
    OCP_USI bId = 0;
    OCP_USI eId = bk.GetInteriorBulkNum();
    for (USI p = 0; p < 2; p++) {

        for (OCP_USI n = bId; n < eId; n++) {
            bvs.P[n] = u[n];
            for (USI j = 0; j < np; j++) {
                bvs.Pj[n * np + j] = bvs.P[n] + bvs.Pc[n * np + j];
            }
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


void IsoT_IMPEC::ResetToLastTimeStep01(Reservoir& rs, OCPControl& ctrl)
{

    // Bulk
    rs.bulk.vs.Ni = rs.bulk.vs.lNi;
    rs.bulk.vs.Pj = rs.bulk.vs.lPj;
    // Bulk Conn

    rs.conn.bcval.upblock      = rs.conn.bcval.lupblock;
    rs.conn.bcval.rho          = rs.conn.bcval.lrho;

    // Iters
    ctrl.ResetIterNRLS();
}

void IsoT_IMPEC::ResetToLastTimeStep02(Reservoir& rs, OCPControl& ctrl)
{
    Bulk&       bk  = rs.bulk;
    BulkVarSet& bvs = bk.vs;
    // Rock
    bvs.rockVp = bvs.lrockVp;
    bvs.poro   = bvs.lporo;
    bvs.poroP  = bvs.lporoP;

    // Fluid
    bvs.phaseNum   = bvs.lphaseNum;
    bvs.Nt         = bvs.lNt;
    bvs.Ni         = bvs.lNi;
    bvs.vf         = bvs.lvf;
    bvs.Pj         = bvs.lPj;
    bvs.phaseExist = bvs.lphaseExist;
    bvs.S          = bvs.lS;
    bvs.vj         = bvs.lvj;
    bvs.xij        = bvs.lxij;
    bvs.rho        = bvs.lrho;
    bvs.xi         = bvs.lxi;
    bvs.mu         = bvs.lmu;

    // derivatives
    bvs.vfP = bvs.lvfP;
    bvs.vfi = bvs.lvfi;

    // Bulk Conn
    rs.conn.bcval.upblock      = rs.conn.bcval.lupblock;
    rs.conn.bcval.rho          = rs.conn.bcval.lrho;

    // Optional Features
    rs.optFeatures.ResetToLastTimeStep();

    // Iters
    ctrl.ResetIterNRLS();
}

void IsoT_IMPEC::UpdateLastTimeStep(Reservoir& rs) const
{

    Bulk&       bk  = rs.bulk;
    BulkVarSet& bvs = bk.vs;

    // Rock
    bvs.lporo   = bvs.poro;
    bvs.lporoP  = bvs.poroP;
    bvs.lrockVp = bvs.rockVp;

    // Fluid
    bvs.lphaseNum   = bvs.phaseNum;
    bvs.lNt         = bvs.Nt;
    bvs.lNi         = bvs.Ni;
    bvs.lvf         = bvs.vf;
    bvs.lP          = bvs.P;
    bvs.lPj         = bvs.Pj;
    bvs.lPc         = bvs.Pc;
    bvs.lphaseExist = bvs.phaseExist;
    bvs.lS          = bvs.S;
    bvs.lvj         = bvs.vj;
    bvs.lxij        = bvs.xij;
    bvs.lrho        = bvs.rho;
    bvs.lxi         = bvs.xi;
    bvs.lmu         = bvs.mu;
    bvs.lkr         = bvs.kr;

    // derivatives
    bvs.lvfP = bvs.vfP;
    bvs.lvfi = bvs.vfi;

    BulkConn& conn = rs.conn;

    conn.bcval.lupblock    = conn.bcval.upblock;
    conn.bcval.lrho        = conn.bcval.rho;

    rs.allWells.UpdateLastTimeStepBHP();
    rs.optFeatures.UpdateLastTimeStep();
}

////////////////////////////////////////////
// IsoT_FIM
////////////////////////////////////////////

void IsoT_FIM::Setup(Reservoir& rs, LinearSystem& ls, const OCPControl& ctrl)
{
    // Allocate memory for reservoir
    AllocateReservoir(rs);
    // Allocate memory for linear system
    AllocateLinearSystem(ls, rs, ctrl);
}

void IsoT_FIM::InitReservoir(Reservoir& rs)
{
    // Calculate initial bulk pressure and temperature and water saturation
    rs.bulk.InitPTSw(50);
    // Initialize rock property
    CalRock(rs.bulk);
    // Initialize fluid properties
    InitFlash(rs.bulk);
    CalKrPc(rs.bulk);
    // Initialize well pressure
    rs.allWells.InitBHP(rs.bulk);
    // Update variables at last time step
    UpdateLastTimeStep(rs);
}

void IsoT_FIM::Prepare(Reservoir& rs, const OCP_DBL& dt)
{
    // Calculate well property at the beginning of next time step
    rs.allWells.PrepareWell(rs.bulk);
    // Calculate initial residual
    CalRes(rs, dt, OCP_TRUE);
}

void IsoT_FIM::AssembleMat(LinearSystem&    ls,
                           const Reservoir& rs,
                           const OCP_DBL&   dt) const
{
    // Assemble matrix
    AssembleMatBulks(ls, rs, dt);
    AssembleMatWells(ls, rs, dt);
    // Assemble rhs -- from residual
    ls.AssembleRhsCopy(res.resAbs);
}

void IsoT_FIM::SolveLinearSystem(LinearSystem& ls,
                                 Reservoir&    rs,
                                 OCPControl&   ctrl)
{
#ifdef DEBUG
    // Check if inf or nan occurs in A and b
    ls.CheckEquation();
#endif // DEBUG

    GetWallTime timer;
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
    
    //ls.OutputLinearSystem("proc" + to_string(CURRENT_RANK) + "_testA_FIM.out",
    //                      "proc" + to_string(CURRENT_RANK) + "_testb_FIM.out");
    //MPI_Barrier(rs.domain.myComm);
    //OCP_ABORT("Stop");
    //
    //ls.OutputSolution("proc" + to_string(CURRENT_RANK) + "_testx_FIM.out");
    // Check if inf or nan occurs in solution
    // ls.CheckSolution();
#endif // DEBUG

    // Get solution from linear system to Reservoir
    timer.Start();
    GetSolution(rs, ls.GetSolution(), ctrl);
    OCPTIME_NRSTEP += timer.Stop() / 1000;   
    // rs.PrintSolFIM(ctrl.workDir + "testPNi.out");
    ls.ClearData();
}

OCP_BOOL IsoT_FIM::UpdateProperty(Reservoir& rs, OCPControl& ctrl)
{
    OCP_DBL& dt = ctrl.current_dt;

    if (!ctrl.Check(rs, {"BulkNi", "BulkP"})) {
        ResetToLastTimeStep(rs, ctrl);
        cout << "Cut time step size and repeat! current dt = " << fixed
             << setprecision(3) << dt << " days\n";
        return OCP_FALSE;
    }

    // Update fluid property
    CalFlash(rs.bulk);
    CalKrPc(rs.bulk);
    // Update rock property
    CalRock(rs.bulk);
    // Update well property
    rs.allWells.CalTrans(rs.bulk);
    rs.allWells.CalFlux(rs.bulk);
    // Update residual
    CalRes(rs, dt, OCP_FALSE);

    return OCP_TRUE;
}

OCP_BOOL IsoT_FIM::FinishNR(Reservoir& rs, OCPControl& ctrl)
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

void IsoT_FIM::FinishStep(Reservoir& rs, OCPControl& ctrl)
{
    rs.CalIPRT(ctrl.GetCurDt());
    rs.CalMaxChange();
    UpdateLastTimeStep(rs);
    ctrl.CalNextTimeStep(rs, {"dP", "dS", "iter"});
}

void IsoT_FIM::AllocateReservoir(Reservoir& rs)
{

    Bulk&       bk  = rs.bulk;
    BulkVarSet& bvs = bk.vs;

    const OCP_USI nb = bvs.nb;
    const USI     np = bvs.np;
    const USI     nc = bvs.nc;

    // Rock
    bvs.poro.resize(nb);
    bvs.rockVp.resize(nb);

    bvs.lporo.resize(nb);
    bvs.lrockVp.resize(nb);

    // derivatives
    bvs.poroP.resize(nb);
    bvs.lporoP.resize(nb);

    // Fluid
    bvs.phaseNum.resize(nb);
    bvs.Nt.resize(nb);
    bvs.Ni.resize(nb * nc);
    bvs.vf.resize(nb);
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

    // derivatives
    bvs.vfP.resize(nb);
    bvs.vfi.resize(nb * nc);
    bvs.rhoP.resize(nb * np);
    bvs.rhox.resize(nb * nc * np);
    bvs.xiP.resize(nb * np);
    bvs.xix.resize(nb * nc * np);
    bvs.muP.resize(nb * np);
    bvs.mux.resize(nb * nc * np);
    bvs.dPcdS.resize(nb * np * np);
    bvs.dKrdS.resize(nb * np * np);

    bvs.lvfP.resize(nb);
    bvs.lvfi.resize(nb * nc);
    bvs.lrhoP.resize(nb * np);
    bvs.lrhox.resize(nb * nc * np);
    bvs.lxiP.resize(nb * np);
    bvs.lxix.resize(nb * nc * np);
    bvs.lmuP.resize(nb * np);
    bvs.lmux.resize(nb * nc * np);
    bvs.ldPcdS.resize(nb * np * np);
    bvs.ldKrdS.resize(nb * np * np);

    // FIM-Specified
    bvs.lendSdP = (nc + 1) * (nc + 1) * np;
    bvs.dSec_dPri.resize(nb * bvs.lendSdP);

    bvs.ldSec_dPri.resize(nb * bvs.lendSdP);

    // BulkConn
    BulkConn& conn = rs.conn;

    conn.bcval.upblock.resize(conn.numConn* np);
    conn.bcval.rho.resize(conn.numConn* np);
    conn.bcval.velocity.resize(conn.numConn* np);


    // NR
    dSNR.resize(nb * np);
    dNNR.resize(nb * nc);
    dPNR.resize(nb);

    // Allocate Residual
    res.Setup_IsoT(bvs.nbI, rs.allWells.numWell, nc);
}

void IsoT_FIM::AllocateLinearSystem(LinearSystem&     ls,
                                    const Reservoir&  rs,
                                    const OCPControl& ctrl)
{
    ls.SetupDomain(rs.domain);
    ls.AllocateRowMem(rs.GetComNum() + 1);
    ls.AllocateColMem();
    ls.SetupLinearSolver(ISOTHERMALMODEL, ctrl.GetWorkDir(), ctrl.GetLsFile());
}

void IsoT_FIM::InitFlash(Bulk& bk)
{
    BulkVarSet& bvs = bk.vs;

    for (OCP_USI n = 0; n < bvs.nb; n++) {
        auto PVT = bk.PVTm.GetPVT(n);

        PVT->InitFlashFIM(bvs.P[n], bvs.Pb[n], bvs.T[n],
                                                &bvs.S[n * bvs.np], bvs.rockVp[n],
                                                bvs.Ni.data() + n * bvs.nc, n);
        for (USI i = 0; i < bvs.nc; i++) {
            bvs.Ni[n * bvs.nc + i] = PVT->GetNi(i);
        }
        PassFlashValue(bk, n);
    }
}

void IsoT_FIM::CalFlash(Bulk& bk)
{
    const BulkVarSet& bvs = bk.vs;

    for (OCP_USI n = 0; n < bvs.nb; n++) {

        bk.PVTm.GetPVT(n)->FlashFIM(bvs.P[n], bvs.T[n], &bvs.Ni[n * bvs.nc], &bvs.S[n * bvs.np], 
                                    bvs.phaseNum[n], &bvs.xij[n * bvs.np * bvs.nc], n);
        PassFlashValue(bk, n);
    }
}

void IsoT_FIM::PassFlashValue(Bulk& bk, const OCP_USI& n)
{
    auto&         bvs = bk.vs;
    const auto    PVT = bk.PVTm.GetPVT(n);

    const auto    np     = bvs.np;
    const auto    nc     = bvs.nc;
    const OCP_USI bIdp   = n * np;

    bvs.phaseNum[n] = 0;
    bvs.Nt[n]       = PVT->GetNt();
    bvs.vf[n]       = PVT->GetVf();

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

            // Derivatives
            bvs.rhoP[bIdp + j] = PVT->GetRhoP(j);
            bvs.xiP[bIdp + j]  = PVT->GetXiP(j);
            bvs.muP[bIdp + j]  = PVT->GetMuP(j);

            for (USI i = 0; i < nc; i++) {
                bvs.xij[bIdp * nc + j * nc + i]  = PVT->GetXij(j, i);
                bvs.rhox[bIdp * nc + j * nc + i] = PVT->GetRhoX(j, i);
                bvs.xix[bIdp * nc + j * nc + i]  = PVT->GetXiX(j, i);
                bvs.mux[bIdp * nc + j * nc + i]  = PVT->GetMuX(j, i);
            }
        }
    }

    bvs.vfP[n] = PVT->GetVfP();
    for (USI i = 0; i < nc; i++) {
        bvs.vfi[n * nc + i] = PVT->GetVfi(i);
    }

    copy(PVT->GetDXsDXp().begin(), PVT->GetDXsDXp().end(), &bvs.dSec_dPri[n * bvs.lendSdP]);
}

void IsoT_FIM::CalKrPc(Bulk& bk) const
{
    BulkVarSet& bvs = bk.vs;
    const USI& np = bvs.np;
    for (OCP_USI n = 0; n < bvs.nb; n++) {
        auto SAT = bk.SATm.GetSAT(n);

        const OCP_USI bId = n * np;
        SAT->CalKrPcFIM(&bvs.S[bId], n);
        copy(SAT->GetKr().begin(), SAT->GetKr().end(), &bvs.kr[bId]);
        copy(SAT->GetPc().begin(), SAT->GetPc().end(), &bvs.Pc[bId]);
        copy(SAT->GetdKrdS().begin(), SAT->GetdKrdS().end(), &bvs.dKrdS[bId * np]);
        copy(SAT->GetdPcdS().begin(), SAT->GetdPcdS().end(), &bvs.dPcdS[bId * np]);
        for (USI j = 0; j < np; j++) bvs.Pj[bId + j] = bvs.P[n] + bvs.Pc[bId + j];
    }
}

void IsoT_FIM::CalRes(Reservoir& rs, const OCP_DBL& dt, const OCP_BOOL& resetRes0)
{
    const Bulk& bk  = rs.bulk;
    const BulkVarSet& bvs = bk.vs;

    const USI              nb    = bvs.nbI;
    const USI              np    = bvs.np;
    const USI              nc    = bvs.nc;
    const USI              len   = nc + 1;
    
    res.SetZero();

    // Bulk to Bulk
    OCP_USI bId, eId, bIdb;    
    // Accumalation Term
    for (OCP_USI n = 0; n < nb; n++) {
        bId             = n * len;
        bIdb            = n * nc;
        res.resAbs[bId] = bvs.rockVp[n] - bvs.vf[n];
        for (USI i = 0; i < nc; i++) {
            res.resAbs[bId + 1 + i] = bvs.Ni[bIdb + i] - bvs.lNi[bIdb + i];
        }
    }

    // Flux Term
    BulkConn&       conn  = rs.conn;
    BulkConnValSet& bcval = conn.bcval;
    USI     cType;
    for (OCP_USI c = 0; c < conn.numConn; c++) {

        bId   = conn.iteratorConn[c].BId();
        eId   = conn.iteratorConn[c].EId();
        cType = conn.iteratorConn[c].Type();
        auto Flux = conn.flux[cType];

        Flux->CalFlux(conn.iteratorConn[c], bk);
        copy(Flux->GetUpblock().begin(), Flux->GetUpblock().end(), &bcval.upblock[c * np]);
        copy(Flux->GetRho().begin(), Flux->GetRho().end(), &bcval.rho[c * np]);
        copy(Flux->GetFluxVj().begin(), Flux->GetFluxVj().end(), &bcval.velocity[c * np]);
               
        if (eId < nb) {
            for (USI i = 0; i < nc; i++) {               
                res.resAbs[bId * len + 1 + i] += dt * Flux->GetFluxNi()[i];
                res.resAbs[eId * len + 1 + i] -= dt * Flux->GetFluxNi()[i];
            }
        }
        else {
            for (USI i = 0; i < nc; i++) {
                res.resAbs[bId * len + 1 + i] += dt * Flux->GetFluxNi()[i];
            }
        }
    }

    // Well to Bulk, Well
    USI wId = nb * len;
    for (const auto& wl : rs.allWells.wells) {
        wl.CalResFIM(wId, res, bk, dt);
    }

    // Calculate RelRes
    OCP_DBL tmp;
    for (OCP_USI n = 0; n < nb; n++) {

        for (USI i = 0; i < len; i++) {
            tmp = fabs(res.resAbs[n * len + i] / bvs.rockVp[n]);
            if (res.maxRelRes_V < tmp) {
                res.maxRelRes_V = tmp;
                res.maxId_V     = n;
            }
            res.resRelV[n] += tmp * tmp;
        }
        res.resRelV[n] = sqrt(res.resRelV[n]);

        for (USI i = 1; i < len; i++) {
            tmp = fabs(res.resAbs[n * len + i] / bvs.Nt[n]);
            if (res.maxRelRes_N < tmp) {
                res.maxRelRes_N = tmp;
                res.maxId_N     = n;
            }
            res.resRelN[n] += tmp * tmp;
        }
        res.resRelN[n] = sqrt(res.resRelN[n]);
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

void IsoT_FIM::AssembleMatBulks(LinearSystem&    ls,
                                const Reservoir& rs,
                                const OCP_DBL&   dt) const
{
    const Bulk& bk  = rs.bulk;
    const BulkVarSet& bvs = bk.vs;

    const USI numWell = rs.GetNumOpenWell();

    const BulkConn& conn   = rs.conn;
    const OCP_USI   nb     = bvs.nbI;
    const USI       np     = bvs.np;
    const USI       nc     = bvs.nc;
    const USI       ncol   = nc + 1;
    const USI       ncol2  = np * nc + np;
    const USI       bsize  = ncol * ncol;
    const USI       bsize2 = ncol * ncol2;

    ls.AddDim(nb);

    vector<OCP_DBL> bmat(bsize, 0);
    // Accumulation term
    for (USI i = 1; i < ncol; i++) {
        bmat[i * ncol + i] = 1;
    }
    for (OCP_USI n = 0; n < nb; n++) {
        bmat[0] = bvs.v[n] * bvs.poroP[n] - bvs.vfP[n];
        for (USI i = 0; i < nc; i++) {
            bmat[i + 1] = -bvs.vfi[n * nc + i];
        }
        ls.NewDiag(n, bmat);
    }

    // flux term
    OCP_USI  bId, eId;
    USI      cType;
    for (OCP_USI c = 0; c < conn.numConn; c++) {

        bId   = conn.iteratorConn[c].BId();
        eId   = conn.iteratorConn[c].EId();
        cType = conn.iteratorConn[c].Type();
        auto Flux = conn.flux[cType];

        Flux->AssembleMatFIM(conn.iteratorConn[c], c, conn.bcval, bk);
        
        bmat = Flux->GetdFdXpB();
        DaABpbC(ncol, ncol, ncol2, 1, Flux->GetdFdXsB().data(), &bvs.dSec_dPri[bId * bsize2], 1,
            bmat.data());
        Dscalar(bsize, dt, bmat.data());
        // Assemble
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

        // End
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
            // ghost grid
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


void IsoT_FIM::AssembleMatWells(LinearSystem&    ls,
                                const Reservoir& rs,
                                const OCP_DBL&   dt) const
{
    for (auto& wl : rs.allWells.wells) {
        wl.AssembleMatFIM(ls, rs.bulk, dt);
    }

    //// for Reinjection
    // for (auto& wG : rs.allWells.wellGroup) {
    //     if (wG.IfReInj()) {
    //         for (auto& prod : rs.allWells.wellGroup[wG.prodGroup].wIdPROD) {
    //             if (rs.allWells.wells[prod].IsOpen()) {
    //                 rs.allWells.wells[prod].AssembleMatReinjection_FIM(rs.bulk, ls,
    //                 dt, rs.allWells.wells,
    //                     wG.wIdINJ);
    //             }
    //         }
    //     }
    // }
}


void IsoT_FIM::GetSolution(Reservoir&        rs,
                           vector<OCP_DBL>&  u,
                           const OCPControl& ctrl)
{
    const auto& domain = rs.domain;
    auto&       bk     = rs.bulk;
    auto&       bvs    = bk.vs;
    const auto  nb     = bvs.nb;
    const auto  np     = bvs.np;
    const auto  nc     = bvs.nc;
    const auto  row    = np * (nc + 1);
    const auto  col    = nc + 1;

    // Well first
    USI wId = bvs.nbI * col;
    for (auto& wl : rs.allWells.wells) {
        if (wl.IsOpen()) {
            wl.SetBHP(wl.BHP() + u[wId]);
            wId += col;
        }
    }

    GetWallTime timerT;         ///< total timer
    GetWallTime timerC;         ///< calculation timer
    OCP_DBL     time_cal = 0;   ///< calculation time
    timerT.Start();
    

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

    OCP_USI bId = 0;
    OCP_USI eId = bk.GetInteriorBulkNum();

    // interior first, ghost second
    for (USI p = 0; p < 2; p++) {
  
        timerC.Start();

		for (OCP_USI n = bId; n < eId; n++) {
			// const vector<OCP_DBL>& scm = satcm[SATNUM[n]];

			chopmin = 1;
			// compute the chop
			fill(dtmp.begin(), dtmp.end(), 0.0);

			DaAxpby(row, col, 1, &bvs.dSec_dPri[n * bvs.lendSdP], u.data() + n * col, 1,
				dtmp.data());

			for (USI j = 0; j < np; j++) {
				choptmp = 1;
				if (fabs(dtmp[j]) > dSmaxlim) {
					choptmp = dSmaxlim / fabs(dtmp[j]);
				}
				else if (bvs.S[n * np + j] + dtmp[j] < 0.0) {
					choptmp = 0.9 * bvs.S[n * np + j] / fabs(dtmp[j]);
				}
				// if (fabs(S[n_np_j] - scm[j]) > TINY &&
				//     (S[n_np_j] - scm[j]) / (choptmp * dtmp[js]) < 0)
				//     choptmp *= min(1.0, -((S[n_np_j] - scm[j]) / (choptmp * dtmp[js])));
				chopmin = min(chopmin, choptmp);
			}

			// dS
			for (USI j = 0; j < np; j++) {
				bvs.S[n * np + j] += chopmin * dtmp[j];
			}

			// dxij   ---- Compositional model only

			if (bk.PVTm.GetMixtureType() == OCPMixtureType::COMP) {
				USI js = np;
				if (bvs.phaseNum[n] >= 3) {
					// num of Hydroncarbon phase >= 2
					OCP_USI bId = 0;
					for (USI j = 0; j < 2; j++) {
						bId = n * np * nc + j * nc;
						for (USI i = 0; i < bvs.nc; i++) {
							bvs.xij[bId + i] += chopmin * dtmp[js];
							js++;
						}
					}
				}
			}

			// dP
			OCP_DBL dP = u[n * col];
			// choptmp = dPmaxlim / fabs(dP);
			// chopmin = min(chopmin, choptmp);
			if (fabs(NRdPmax) < fabs(dP)) NRdPmax = dP;
			bvs.P[n] += dP; // seems better
			dPNR[n] = dP;

			// dNi
			for (USI i = 0; i < nc; i++) {
				dNNR[n * nc + i] = u[n * col + 1 + i] * chopmin;
				if (fabs(NRdNmax) < fabs(dNNR[n * nc + i]) / bvs.Nt[n])
					NRdNmax = dNNR[n * nc + i] / bvs.Nt[n];

				bvs.Ni[n * nc + i] += dNNR[n * nc + i];

				// if (bvs.Ni[n * nc + i] < 0 && bvs.Ni[n * nc + i] > -1E-3) {
				//     bvs.Ni[n * nc + i] = 1E-20;
				// }
			}
		}

        time_cal += timerC.Stop();

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

    OCPTIME_COMM_P2P += (timerT.Stop() - time_cal) / 1000;
    OCPTIME_NRSTEPC  += time_cal / 1000;
}

void IsoT_FIM::ResetToLastTimeStep(Reservoir& rs, OCPControl& ctrl)
{
    Bulk&       bk = rs.bulk;
    BulkVarSet& bvs = bk.vs;

    // Rock
    bvs.poro   = bvs.lporo;
    bvs.poroP  = bvs.lporoP;
    bvs.rockVp = bvs.lrockVp;
    // Fluid
    bvs.phaseNum   = bvs.lphaseNum;
    bvs.Nt         = bvs.lNt;
    bvs.Ni         = bvs.lNi;
    bvs.vf         = bvs.lvf;
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
    // derivatives
    bvs.vfP     = bvs.lvfP;
    bvs.vfi     = bvs.lvfi;
    bvs.rhoP    = bvs.lrhoP;
    bvs.rhox    = bvs.lrhox;
    bvs.xiP     = bvs.lxiP;
    bvs.xix     = bvs.lxix;
    bvs.muP     = bvs.lmuP;
    bvs.mux     = bvs.lmux;
    bvs.dPcdS   = bvs.ldPcdS;
    bvs.dKrdS   = bvs.ldKrdS;
    // FIM-Specified
    bvs.dSec_dPri    = bvs.ldSec_dPri;

    // Wells
    rs.allWells.ResetBHP();
    rs.allWells.CalTrans(bk);
    rs.allWells.CaldG(bk);
    rs.allWells.CalFlux(bk);

    // Optional Features
    rs.optFeatures.ResetToLastTimeStep();

    // Iters
    ctrl.ResetIterNRLS();

    // Residual
    CalRes(rs, ctrl.GetCurDt(), OCP_TRUE);
}

void IsoT_FIM::UpdateLastTimeStep(Reservoir& rs) const
{
    BulkVarSet& bvs = rs.bulk.vs;
    // Rock
    bvs.lporo   = bvs.poro;
    bvs.lporoP  = bvs.poroP;
    bvs.lrockVp = bvs.rockVp;

    // Fluid
    bvs.lphaseNum   = bvs.phaseNum;
    bvs.lNt         = bvs.Nt;
    bvs.lNi         = bvs.Ni;
    bvs.lvf         = bvs.vf;
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

    // derivatives
    bvs.lvfP     = bvs.vfP;
    bvs.lvfi     = bvs.vfi;
    bvs.lrhoP    = bvs.rhoP;
    bvs.lrhox    = bvs.rhox;
    bvs.lxiP     = bvs.xiP;
    bvs.lxix     = bvs.xix;
    bvs.lmuP     = bvs.muP;
    bvs.lmux     = bvs.mux;
    bvs.ldPcdS = bvs.dPcdS;
    bvs.ldKrdS  = bvs.dKrdS;

    // FIM-Specified
    bvs.ldSec_dPri    = bvs.dSec_dPri;

    rs.allWells.UpdateLastTimeStepBHP();
    rs.optFeatures.UpdateLastTimeStep();
}

////////////////////////////////////////////
// IsoT_AIMc
////////////////////////////////////////////

void IsoT_AIMc::Setup(Reservoir& rs, LinearSystem& ls, const OCPControl& ctrl)
{
    // Allocate Bulk and BulkConn Memory
    AllocateReservoir(rs);
    // Setup neighbor
    SetupNeighbor(rs);
    // Allocate memory for internal matrix structure
    IsoT_FIM::AllocateLinearSystem(ls, rs, ctrl);
}


void IsoT_AIMc::SetupNeighbor(Reservoir& rs)
{
    // Note: 
    // for interior bulk: neighbor stores their all 1-neighbors
    // for ghost    bulk: neighbor stores their 1-neighbors belonging to current process

    const OCP_USI nb   = rs.GetBulkNum();
    OCP_USI       bId, eId;
    BulkConn&     conn = rs.conn;
    
    conn.neighbor.resize(nb);
    for (const auto& c : conn.iteratorConn) {
        bId = c.BId();
        eId = c.EId();

        conn.neighbor[bId].push_back(eId);
        conn.neighbor[eId].push_back(bId);
    }
}


void IsoT_AIMc::InitReservoir(Reservoir& rs) const
{
    rs.bulk.InitPTSw(50);

    CalRock(rs.bulk);

    IsoT_IMPEC::InitFlash(rs.bulk);
    IsoT_IMPEC::CalKrPc(rs.bulk);

    rs.allWells.InitBHP(rs.bulk);

    UpdateLastTimeStep(rs);
}

void IsoT_AIMc::Prepare(Reservoir& rs, const OCP_DBL& dt)
{
    rs.allWells.PrepareWell(rs.bulk);
    CalRes(rs, dt, OCP_TRUE);

    // Set FIM Bulk
    rs.CalCFL(dt, OCP_FALSE);
    rs.allWells.SetupWellBulk(rs.bulk);
    SetFIMBulk(rs);
    //  Calculate FIM Bulk properties
    CalFlashI(rs.bulk);
    CalKrPcI(rs.bulk);

    UpdateLastTimeStep(rs);
}

void IsoT_AIMc::AssembleMat(LinearSystem&    ls,
                            const Reservoir& rs,
                            const OCP_DBL&   dt) const
{
    AssembleMatBulks(ls, rs, dt);
    IsoT_FIM::AssembleMatWells(ls, rs, dt);
    ls.AssembleRhsCopy(res.resAbs);
}

void IsoT_AIMc::SolveLinearSystem(LinearSystem& ls, Reservoir& rs, OCPControl& ctrl)
{
#ifdef DEBUG
    ls.CheckEquation();
#endif // DEBUG

    GetWallTime timer;

    timer.Start();
    ls.CalCommTerm(rs.GetNumOpenWell());
    ls.AssembleMatLinearSolver();
    OCPTIME_ASSEMBLE_MAT_FOR_LS += timer.Stop() / 1000;

    timer.Start();
    int status = ls.Solve();
    if (status < 0) {
        status = ls.GetNumIters();
    }

#ifdef DEBUG
    //OCP_INT myrank = rs.domain.myrank;
    //ls.OutputLinearSystem("proc" + to_string(CURRENT_RANK) + "_testA_AIMc.out",
    //                      "proc" + to_string(CURRENT_RANK) + "_testb_AIMc.out");
    //MPI_Barrier(rs.domain.myComm);
    //ls.OutputSolution("proc" + to_string(CURRENT_RANK) + "_testx_AIMc.out");
    // ls.CheckSolution();
#endif // DEBUG

    OCPTIME_LSOLVER += timer.Stop() / 1000;
    ctrl.UpdateIterLS(status);
    ctrl.UpdateIterNR();

    timer.Start();
    GetSolution(rs, ls.GetSolution(), ctrl);
    OCPTIME_NRSTEP += timer.Stop() / 1000;
    ls.ClearData();
}

OCP_BOOL IsoT_AIMc::UpdateProperty(Reservoir& rs, OCPControl& ctrl)
{
    const OCP_DBL& dt = ctrl.current_dt;

    // First check: Ni check and bulk Pressure check
    if (!ctrl.Check(rs, {"BulkNi", "BulkP"})) {
        ResetToLastTimeStep(rs, ctrl);
        cout << "Cut time step size and repeat! current dt = " << fixed
             << setprecision(3) << dt << " days\n";
        return OCP_FALSE;
    }

    CalFlashI(rs.bulk);
    CalFlashEp(rs.bulk);
    CalKrPcI(rs.bulk);

    CalRock(rs.bulk);

    rs.allWells.CalTrans(rs.bulk);
    rs.allWells.CalFlux(rs.bulk);

    CalRes(rs, dt, OCP_FALSE);
    return OCP_TRUE;
}

OCP_BOOL IsoT_AIMc::FinishNR(Reservoir& rs, OCPControl& ctrl)
{
    NRdSmax = CalNRdSmax();
    // const OCP_DBL NRdNmax = rs.GetNRdNmax();

#ifdef DEBUG
    // cout << "### DEBUG: Residuals = " << setprecision(3) << scientific
    //      << resAIMc.maxRelRes0_V << "  " << resAIMc.maxRelRes_V << "  "
    //      << resAIMc.maxRelRes_N << "  " << NRdPmax << "  " << NRdSmax << endl;
#endif

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
            CalFlashEa(rs.bulk);
            CalKrPcE(rs.bulk);
            return OCP_TRUE;
        }

    } else if (ctrl.iterNR > ctrl.ctrlNR.maxNRiter) {
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

/// Finish a time step.
void IsoT_AIMc::FinishStep(Reservoir& rs, OCPControl& ctrl) const
{
    rs.CalIPRT(ctrl.GetCurDt());
    rs.CalMaxChange();
    UpdateLastTimeStep(rs);
    ctrl.CalNextTimeStep(rs, {"dP", "dS", "iter"});
}

/// Allocate memory for reservoir
void IsoT_AIMc::AllocateReservoir(Reservoir& rs)
{
    IsoT_FIM::AllocateReservoir(rs);

    Bulk&         bk  = rs.bulk;
    BulkVarSet&   bvs = bk.vs;
    const OCP_USI nb  = bvs.nb;
    const USI     np  = bvs.np;
    const USI     nc  = bvs.nc;

    bvs.vj.resize(nb * np);
    bvs.lvj.resize(nb * np);

    bk.xijNR.resize(nb * np * nc);
    bk.cfl.resize(nb * np);
    bk.bulkTypeAIM.Setup(nb);
}

void IsoT_AIMc::SetFIMBulk(Reservoir& rs)
{
    // IMPORTANT: implicity of the same grid in different processes should be consistent
    
    const OCP_INT nlayers = 2;

    // We just consider at most 1 layer neighbor now

    Bulk&           bk  = rs.bulk;
    BulkVarSet&     bvs = bk.vs;
    const BulkConn& conn = rs.conn;
    const OCP_USI   nb   = bvs.nbI;
    const USI       np   = bvs.np;
    const USI       nc   = bvs.nc;

    // all impec
    bk.bulkTypeAIM.Init(-1);

    OCP_USI  bIdp, bIdc;
    OCP_BOOL flag;

    for (OCP_USI n = 0; n < nb; n++) {
        bIdp = n * np;
        bIdc = n * nc;
        flag = OCP_FALSE;
        // CFL
        for (USI j = 0; j < np; j++) {
            if (bk.cfl[bIdp + j] > 0.8) {
                flag = OCP_TRUE;
                break;
            }
        }
        // Volume error
        if (!flag) {
            if ((fabs(bvs.vf[n] - bvs.rockVp[n]) / bvs.rockVp[n]) > 1E-3) {
                flag = OCP_TRUE;
            }
        }

        // NR Step
        if (!flag && OCP_FALSE) {
            // dP
            if (fabs(dPNR[n] / bvs.P[n]) > 1E-3) {
                flag = OCP_TRUE;
            }
            // dNi
            if (!flag) {
                for (USI i = 0; i < bvs.nc; i++) {
                    if (fabs(dNNR[bIdc + i] / bvs.Ni[bIdc + i]) > 1E-3) {
                        flag = OCP_TRUE;
                        break;
                    }
                }
            }
        }

        if (flag) {
            SetKNeighbor(conn.neighbor, n, bk.bulkTypeAIM, nlayers);
        }
    }

    // add WellBulk's 1-neighbor as Implicit bulk
    for (auto& p : bk.wellBulkId) {
        SetKNeighbor(conn.neighbor, p, bk.bulkTypeAIM, nlayers);
    }

    // exchange information of implicity of grid
    const Domain& domain = rs.domain;

    vector<vector<OCP_INT>> recv_buffer(domain.numRecvProc);
    for (USI i = 0; i < domain.numRecvProc; i++) {
        const vector<OCP_USI>& rel = domain.recv_element_loc[i];
        vector<OCP_INT>&       r   = recv_buffer[i];
        r.resize(rel[2] - rel[1]);
        MPI_Irecv(&r[0], rel[2] - rel[1], MPI_INT, rel[0], 0, domain.myComm, &domain.recv_request[i]);
    }

    vector<vector<OCP_INT>> send_buffer(domain.numSendProc);
    for (USI i = 0; i < domain.numSendProc; i++) {
        const vector<OCP_USI>& sel = domain.send_element_loc[i];
        vector<OCP_INT>&       s   = send_buffer[i];
        s.resize(sel.size());
        s[0] = sel[0];
        for (USI j = 1; j < sel.size(); j++) {
            s[j] = bk.bulkTypeAIM.GetBulkType(sel[j]);
        }
        MPI_Isend(s.data() + 1, s.size() - 1, MPI_INT, s[0], 0, domain.myComm, &domain.send_request[i]);
    }

    MPI_Waitall(domain.numRecvProc, domain.recv_request.data(), MPI_STATUS_IGNORE);
    for (USI i = 0; i < domain.numRecvProc; i++) {
        const vector<OCP_USI>& rel = domain.recv_element_loc[i];
        const vector<OCP_INT>& r   = recv_buffer[i];

        for (OCP_USI n = 0; n < rel[2] - rel[1]; n++) {
            SetKNeighbor(conn.neighbor, n + rel[1], bk.bulkTypeAIM, r[n]);
        }
    }

    MPI_Waitall(domain.numSendProc, domain.send_request.data(), MPI_STATUS_IGNORE);

    // Check Consistency
    for (USI i = 0; i < domain.numRecvProc; i++) {
        const vector<OCP_USI>& rel = domain.recv_element_loc[i];
        vector<OCP_INT>&       r   = recv_buffer[i];
        r.resize(rel[2] - rel[1]);
        MPI_Irecv(&r[0], rel[2] - rel[1], MPI_INT, rel[0], 0, domain.myComm, &domain.recv_request[i]);
    }

    for (USI i = 0; i < domain.numSendProc; i++) {
        const vector<OCP_USI>& sel = domain.send_element_loc[i];
        vector<OCP_INT>&       s   = send_buffer[i];
        s.resize(sel.size());
        s[0] = sel[0];
        for (USI j = 1; j < sel.size(); j++) {
            s[j] = bk.bulkTypeAIM.GetBulkType(sel[j]);
        }
        MPI_Isend(s.data() + 1, s.size() - 1, MPI_INT, s[0], 0, domain.myComm, &domain.send_request[i]);
    }

    MPI_Waitall(domain.numRecvProc, domain.recv_request.data(), MPI_STATUS_IGNORE);
     
    for (USI i = 0; i < domain.numRecvProc; i++) {
        const vector<OCP_USI>& rel = domain.recv_element_loc[i];
        vector<OCP_INT>&       r   = recv_buffer[i];
        for (OCP_USI n = 0; n < rel[2] - rel[1]; n++) {
            bk.bulkTypeAIM.SetBulkType(n + rel[1], r[n]);     // Maybe not a good idea
        }
    }

    MPI_Waitall(domain.numSendProc, domain.send_request.data(), MPI_STATUS_IGNORE);

    if (OCP_TRUE) {
        cout << fixed << setprecision(2) << "Rank " << CURRENT_RANK << "  " << bk.bulkTypeAIM.GetNumFIMBulk() * 1.0 / bvs.nb * 100 << "% " << endl;
    }
}


void IsoT_AIMc::SetKNeighbor(const vector<vector<OCP_USI>>& neighbor, const OCP_USI& p, BulkTypeAIM& tar, OCP_INT k)
{
    tar.SetBulkType(p, max(k, tar.GetBulkType(p)));
    if (k > 0) {
        k--;
        for (const auto& v : neighbor[p]) {
            SetKNeighbor(neighbor, v, tar, k);
        }
    }
}


void IsoT_AIMc::CalFlashEp(Bulk& bk)
{
    const BulkVarSet& bvs = bk.vs;
    const OCP_USI&    nb  = bvs.nb;
    const USI&        np  = bvs.np;
    const USI&        nc  = bvs.nc;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bk.bulkTypeAIM.IfIMPECbulk(n)) {
            // Explicit bulk

            bk.PVTm.GetPVT(n)->FlashIMPEC(bvs.P[n], bvs.T[n], &bvs.Ni[n * nc], bvs.phaseNum[n],
                                          &bk.xijNR[n * np * nc], n);
            // bvs.PassFlashValueAIMcEp(n);
            PassFlashValueEp(bk, n);
        }
    }
}

void IsoT_AIMc::CalFlashEa(Bulk& bk)
{
    const BulkVarSet& bvs = bk.vs;
    const OCP_USI&    nb  = bvs.nb;
    const USI&        np  = bvs.np;
    const USI&        nc  = bvs.nc;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bk.bulkTypeAIM.IfIMPECbulk(n)) {
            // Explicit bulk

            bk.PVTm.GetPVT(n)->FlashIMPEC(bvs.P[n], bvs.T[n], &bvs.Ni[n * nc], bvs.phaseNum[n], 
                                          &bvs.xij[n * np * nc], n);
            // bvs.PassFlashValueAIMcEa(n);

            IsoT_IMPEC::PassFlashValue(bk, n);
        }
    }
}

void IsoT_AIMc::CalFlashI(Bulk& bk)
{
    BulkVarSet&    bvs = bk.vs;
    const OCP_USI& nb  = bvs.nb;
    const USI&     np  = bvs.np;
    const USI&     nc  = bvs.nc;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bk.bulkTypeAIM.IfFIMbulk(n)) {
            // Implicit bulk

            bk.PVTm.GetPVT(n)->FlashFIM(bvs.P[n], bvs.T[n], &bvs.Ni[n * nc], &bvs.S[n * np], 
                                        bvs.phaseNum[n], &bvs.xij[n * np * nc], n);
            IsoT_FIM::PassFlashValue(bk, n);
            for (USI j = 0; j < np; j++) {
                bvs.vj[n * np + j] = bvs.vf[n] * bvs.S[n * np + j];
            }
        }
    }
}

void IsoT_AIMc::PassFlashValueEp(Bulk& bk, const OCP_USI& n)
{
    // only var about volume needs, some flash var also
    OCP_FUNCNAME;
    auto&         bvs  = bk.vs;
    const auto    PVT  = bk.PVTm.GetPVT(n);

    const auto    np   = bvs.np;
    const auto    nc   = bvs.nc;
    const OCP_USI bIdp = n * np;

    bvs.Nt[n]  = PVT->GetNt();
    bvs.vf[n]  = PVT->GetVf();
    bvs.vfP[n] = PVT->GetVfP();
    for (USI i = 0; i < nc; i++) {
        bvs.vfi[n * nc + i] = PVT->GetVfi(i);
    }

    bvs.phaseNum[n] = 0;
    for (USI j = 0; j < np; j++) {
        if (PVT->GetPhaseExist(j)) {
            bvs.phaseNum[n]++;

            // IMPORTANT -- need for next Flash
            // But xij in nonlinear equations has been modified
            for (USI i = 0; i < nc; i++) {
                bk.xijNR[bIdp * nc + j * nc + i] = PVT->GetXij(j, i);
            }
        }
    }
}

void IsoT_AIMc::CalKrPcE(Bulk& bk)
{
    BulkVarSet&    bvs = bk.vs;
    const OCP_USI& nb = bvs.nb;
    const USI&     np = bvs.np;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bk.bulkTypeAIM.IfIMPECbulk(n)) {

            auto SAT = bk.SATm.GetSAT(n);
            // Explicit bulk
            const OCP_USI bId = n * np;
            SAT->CalKrPc(&bvs.S[bId], n);
            copy(SAT->GetKr().begin(), SAT->GetKr().end(), &bvs.kr[bId]);
            copy(SAT->GetPc().begin(), SAT->GetPc().end(), &bvs.Pc[bId]);
            for (USI j = 0; j < np; j++) bvs.Pj[bId + j] = bvs.P[n] + bvs.Pc[bId + j];
        }
    }
}

void IsoT_AIMc::CalKrPcI(Bulk& bk)
{
    BulkVarSet&    bvs = bk.vs;
    const OCP_USI& nb = bvs.nb;
    const USI&     np = bvs.np;

    for (OCP_USI n = 0; n < nb; n++) {
        if (bk.bulkTypeAIM.IfFIMbulk(n)) {
            auto SAT = bk.SATm.GetSAT(n);
            // Implicit bulk
            const OCP_USI bId = n * np;
            SAT->CalKrPcFIM(&bvs.S[bId], n);
            copy(SAT->GetKr().begin(), SAT->GetKr().end(), &bvs.kr[bId]);
            copy(SAT->GetPc().begin(), SAT->GetPc().end(), &bvs.Pc[bId]);
            copy(SAT->GetdKrdS().begin(), SAT->GetdKrdS().end(), &bvs.dKrdS[bId * np]);
            copy(SAT->GetdPcdS().begin(), SAT->GetdPcdS().end(), &bvs.dPcdS[bId * np]);
            for (USI j = 0; j < np; j++) bvs.Pj[bId + j] = bvs.P[n] + bvs.Pc[bId + j];
        }
    }
}

void IsoT_AIMc::AssembleMatBulks(LinearSystem&    ls,
                                 const Reservoir& rs,
                                 const OCP_DBL&   dt) const
{
    const USI numWell = rs.GetNumOpenWell();

    const Bulk& bk = rs.bulk;
    const BulkVarSet& bvs = bk.vs;
    const BulkConn& conn    = rs.conn;
    const OCP_USI   nb      = bvs.nbI;
    const USI       np      = bvs.np;
    const USI       nc      = bvs.nc;
    const USI       ncol    = nc + 1;
    const USI       ncol2   = np * nc + np;
    const USI       bsize   = ncol * ncol;
    const USI       bsize2  = ncol * ncol2;

    ls.AddDim(nb);

    vector<OCP_DBL> bmat(bsize, 0);
    // Accumulation term
    for (USI i = 1; i < nc + 1; i++) {
        bmat[i * ncol + i] = 1;
    }
    for (OCP_USI n = 0; n < nb; n++) {
        bmat[0] = bvs.v[n] * bvs.poroP[n] - bvs.vfP[n];
        for (USI i = 0; i < nc; i++) {
            bmat[i + 1] = -bvs.vfi[n * nc + i];
        }
        ls.NewDiag(n, bmat);
    }

    // flux term
    OCP_BOOL          bIdFIM, eIdFIM;
    OCP_USI           bId, eId;
    USI               cType;
    
   
    for (OCP_USI c = 0; c < conn.numConn; c++) {
        bId    = conn.iteratorConn[c].BId();
        eId    = conn.iteratorConn[c].EId();
        cType  = conn.iteratorConn[c].Type();
        auto Flux = conn.flux[cType];

        if (bk.bulkTypeAIM.IfFIMbulk(bId))  bIdFIM = OCP_TRUE;
        else                                bIdFIM = OCP_FALSE;

        if (bk.bulkTypeAIM.IfFIMbulk(eId))  eIdFIM = OCP_TRUE;
        else                                eIdFIM = OCP_FALSE;

        Flux->AssembleMatAIM(conn.iteratorConn[c], c, conn.bcval, bk);

        // Assemble
        bmat = Flux->GetdFdXpB();
        if (bIdFIM) {
            DaABpbC(ncol, ncol, ncol2, 1, Flux->GetdFdXsB().data(), &bvs.dSec_dPri[bId * bsize2],
                    1, bmat.data());
        }
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
            OCP_ABORT("INF or NAN in bmat !");
        }
#endif

        // End
        bmat = Flux->GetdFdXpE();
        if (eIdFIM) {
            DaABpbC(ncol, ncol, ncol2, 1, Flux->GetdFdXsE().data(), &bvs.dSec_dPri[eId * bsize2],
                    1, bmat.data());
        }
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
            ls.NewOffDiag(bId, eId + numWell, bmat);
        }

   
#ifdef OCP_NANCHECK
        if (!CheckNan(bmat.size(), &bmat[0])) {
            OCP_ABORT("INF or NAN in bmat !");
        }
#endif
    }
}

void IsoT_AIMc::GetSolution(Reservoir&             rs,
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
    const USI       col    = nc + 1;

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

    OCP_USI bId = 0;
    OCP_USI eId = bk.GetInteriorBulkNum();

    for (USI p = 0; p < 2; p++) {

        for (OCP_USI n = bId; n < eId; n++) {
            if (bk.bulkTypeAIM.IfIMPECbulk(n)) {
                // IMPEC Bulk
                // Pressure
                const OCP_DBL dP = u[n * col];
                NRdPmax          = max(NRdPmax, fabs(dP));
                bvs.P[n]          += dP; // seems better
                dPNR[n]          = dP;
                // Ni
                for (USI i = 0; i < nc; i++) {
                    dNNR[n * nc + i] = u[n * col + 1 + i];
                    bvs.Ni[n * nc + i] += dNNR[n * nc + i];

                    // if (bvs.Ni[n * nc + i] < 0 && bvs.Ni[n * nc + i] > -1E-3) {
                    //     bvs.Ni[n * nc + i] = 1E-20;
                    // }
                }
                // Pj
                for (USI j = 0; j < np; j++) {
                    bvs.Pj[n * np + j] = bvs.P[n] + bvs.Pc[n * np + j];
                }
                continue;
            }

            chopmin = 1;
            // compute the chop
            fill(dtmp.begin(), dtmp.end(), 0.0);
            DaAxpby(row, col, 1, &bvs.dSec_dPri[n * bvs.lendSdP],
                u.data() + n * col, 1, dtmp.data());

            for (USI j = 0; j < np; j++) {
                choptmp = 1;
                if (fabs(dtmp[j]) > dSmaxlim) {
                    choptmp = dSmaxlim / fabs(dtmp[j]);
                }
                else if (bvs.S[n * np + j] + dtmp[j] < 0.0) {
                    choptmp = 0.9 * bvs.S[n * np + j] / fabs(dtmp[j]);
                }

                // if (fabs(S[n * np + j] - scm[j]) > TINY &&
                //     (S[n * np + j] - scm[j]) / (choptmp * dtmp[js]) < 0)
                //     choptmp *= min(1.0, -((S[n * np + j] - scm[j]) / (choptmp * dtmp[js])));

                chopmin = min(chopmin, choptmp);
            }

            // dS
            for (USI j = 0; j < np; j++) {
                bvs.S[n * np + j] += chopmin * dtmp[j];
            }

            // dxij   ---- Compositional model only
            if (bk.PVTm.GetMixtureType() == OCPMixtureType::COMP) {
                USI js = np;
                if (bvs.phaseNum[n] >= 3) {
                    OCP_USI bId = 0;
                    for (USI j = 0; j < 2; j++) {
                        bId = n * np * nc + j * nc;
                        for (USI i = 0; i < bvs.nc; i++) {
                            bvs.xij[bId + i] += chopmin * dtmp[js];
                            js++;
                        }
                    }
                }
            }

            // dP
            OCP_DBL dP = u[n * col];
            if (fabs(NRdPmax) < fabs(dP)) NRdPmax = dP;
            bvs.P[n] += dP; // seems better
            dPNR[n] = dP;

            // dNi
            for (USI i = 0; i < nc; i++) {
                dNNR[n * nc + i] = u[n * col + 1 + i] * chopmin;
                if (fabs(NRdNmax) < fabs(dNNR[n * nc + i]) / bvs.Nt[n])
                    NRdNmax = dNNR[n * nc + i] / bvs.Nt[n];

                bvs.Ni[n * nc + i] += dNNR[n * nc + i];

                // if (bvs.Ni[n * nc + i] < 0 && bvs.Ni[n * nc + i] > -1E-3) {
                //     bvs.Ni[n * nc + i] = 1E-20;
                // }
            }
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

void IsoT_AIMc::ResetToLastTimeStep(Reservoir& rs, OCPControl& ctrl)
{
    rs.bulk.vs.vj    = rs.bulk.vs.lvj;
    rs.bulk.xijNR = rs.bulk.vs.lxij;
    IsoT_FIM::ResetToLastTimeStep(rs, ctrl);

    // all FIM
    rs.bulk.bulkTypeAIM.Init(0);
    CalFlashI(rs.bulk);
    CalKrPcI(rs.bulk);

    //if (OCP_TRUE) {
    //    cout << "Rank " << CURRENT_RANK << "  " << rs.bulk.bulkTypeAIM.GetNumFIMBulk() * 1.0 / rs.bulk.numBulk * 100 << "% " << endl;
    //}
}

void IsoT_AIMc::UpdateLastTimeStep(Reservoir& rs) const
{
    IsoT_FIM::UpdateLastTimeStep(rs);

    rs.bulk.vs.lvj   = rs.bulk.vs.vj;
    rs.bulk.xijNR = rs.bulk.vs.xij;
}

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Nov/01/2021      Create file                          */
/*  Chensong Zhang      Jan/08/2022      Update output                        */
/*----------------------------------------------------------------------------*/
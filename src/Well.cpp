/*! \file    Well.cpp
 *  \brief   Well class definition
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "Well.hpp"
#include <cmath>

void Well::InputPerfo(const WellParam& well, const Domain& domain, const USI& wId)
{
    OCP_FUNCNAME;

    numPerf = domain.well2Bulk[wId].size();
    perf.resize(numPerf);
    USI pp = 0;
    for (USI p = 0; p < well.I_perf.size(); p++) {

        const OCP_USI tmpI = well.I_perf[p] - 1;
        const OCP_USI tmpJ = well.J_perf[p] - 1;
        const OCP_USI tmpK = well.K_perf[p] - 1;
        const OCP_INT loc = domain.GetPerfLocation(wId, tmpI, tmpJ, tmpK);
        if (loc < 0) {
            continue;
        }
        perf[pp].location   = loc;
        perf[pp].I          = tmpI;
        perf[pp].J          = tmpJ;
        perf[pp].K          = tmpK;
        perf[pp].WI         = well.WI[p];
        perf[pp].radius     = well.diameter[p] / 2.0;
        perf[pp].kh         = well.kh[p];
        perf[pp].skinFactor = well.skinFactor[p];
        if (well.direction[p] == "X" || well.direction[p] == "x") {
            perf[pp].direction = X_DIRECTION;
        } else if (well.direction[p] == "Y" || well.direction[p] == "y") {
            perf[pp].direction = Y_DIRECTION;
        } else if (well.direction[p] == "Z" || well.direction[p] == "z") {
            perf[pp].direction = Z_DIRECTION;
        } else {
            OCP_ABORT("Wrong direction of perforations!");
        }
        pp++;
    }
    OCP_ASSERT(pp = numPerf, "Wrong Perf!");
}


void Well::Setup(const Bulk& bk, const vector<SolventINJ>& sols)
{
    OCP_FUNCNAME;

    nc       = bk.numCom;
    np       = bk.numPhase;
    mixture  = bk.flashCal[0]->GetMixture();

    qi_lbmol.resize(nc);
    factor.resize(nc);

    for (auto& opt : optSet) {
        if (!opt.state) continue;
        if (!bk.ifThermal) {
            opt.injTemp = bk.rsTemp;
        }
    }

    SetupUnit();
    SetupOpts(sols);

    // Perf
    for (USI p = 0; p < numPerf; p++) {
        perf[p].state      = OPEN;
        perf[p].depth      = bk.depth[perf[p].location];
        perf[p].multiplier = 1;
        perf[p].qi_lbmol.resize(nc);
        perf[p].transj.resize(np);
        perf[p].qj_ft3.resize(np);
    }
    // dG
    dG.resize(numPerf, 0);
    ldG = dG;

    if (depth < 0) depth = perf[0].depth;

    CalWI_Peaceman(bk);
    // test
    // ShowPerfStatus(bk);
}


void Well::SetupUnit()
{
    if (mixture->OilIndex() >= 0) unitConvert.push_back(CONV1);
    if (mixture->GasIndex() >= 0) unitConvert.push_back(1000);
    if (mixture->WatIndex() >= 0) unitConvert.push_back(CONV1);

    OCP_ASSERT(unitConvert.size() == np, "WRONG unitConvert");
}


void Well::SetupOpts(const vector<SolventINJ>& sols)
{
    for (auto& opt : optSet) {
        if (!opt.state) continue;
        if (opt.type == INJ) {
            SetupOptsInj(opt, sols);
        }
        else if (opt.type == PROD) {
            SetupOptsProd(opt);
        }
        else {
            OCP_ABORT("Inavailable Well Type!");
        }
    }
}


void Well::SetupOptsInj(WellOpt& opt, const vector<SolventINJ>& sols)
{
    const auto g = mixture->GasIndex();
    const auto w = mixture->WatIndex();
 
    if (opt.fluidType == "WAT") {     
        if (w < 0) OCP_ABORT("WRONG INJECTED FLUID -- NO WATER!");
        opt.injZi.assign(nc, 0);
        opt.injPhase     = PhaseType::wat;
        opt.injZi.back() =  1.0;
        opt.factorINJ    =  unitConvert[w] / mixture->CalVmStd(Psurf, Tsurf, &opt.injZi[0], opt.injPhase);
        opt.maxRate      *= opt.factorINJ;
    }
    else if (!sols.empty())
    {
        opt.injPhase  = PhaseType::gas;
        for (auto& s : sols) {
            if (opt.fluidType == s.name) {
                opt.injZi = s.data;
                opt.injZi.resize(nc);
            }
        }
        if (opt.injZi.size() != nc) {
            OCP_ABORT("Inavailable INJECTED FLUID -- " + opt.fluidType + "!");
        }
        opt.factorINJ = unitConvert[g] / mixture->CalVmStd(Psurf, Tsurf, &opt.injZi[0], opt.injPhase);
        opt.maxRate   *= opt.factorINJ;
    }
    else if (opt.fluidType == "GAS") {
        if (g < 0) OCP_ABORT("WRONG INJECTED FLUID -- NO GAS!");
        opt.injZi.assign(nc, 0);
        opt.injPhase  = PhaseType::gas;
        opt.injZi[g]  =  1.0;
        opt.factorINJ = unitConvert[g] / mixture->CalVmStd(Psurf, Tsurf, &opt.injZi[0], opt.injPhase);
        opt.maxRate   *= opt.factorINJ;
    }
    else {
        OCP_ABORT("Inavailable INJECTED FLUID!");
    }
}


void Well::SetupOptsProd(WellOpt& opt)
{
    const auto o = mixture->OilIndex();
    const auto g = mixture->GasIndex();
    const auto w = mixture->WatIndex();
    const auto l = mixture->LiquidIndex();

    opt.prodPhaseWeight.assign(np, 0);
    switch (opt.optMode) {
    case BHP_MODE:
        break;
    case ORATE_MODE:
        if (o < 0) OCP_ABORT("WRONG PRODUCTED FLUID -- NO OIL!");
        opt.prodPhaseWeight[o] = 1.0;
        break;
    case GRATE_MODE:
        if (g < 0) OCP_ABORT("WRONG PRODUCTED FLUID -- NO GAS!");
        opt.prodPhaseWeight[g] = 1.0;
        break;
    case WRATE_MODE:
        if (w < 0) OCP_ABORT("WRONG PRODUCTED FLUID -- NO WATER!");
        opt.prodPhaseWeight[w] = 1.0;
        break;
    case LRATE_MODE:
        if (l.size() == 0) OCP_ABORT("WRONG PRODUCTED FLUID -- NO LIQUID!");
        for (auto& i : l)  opt.prodPhaseWeight[i] = 1.0;
        break;
    default:
        OCP_ABORT("WRONG Opt Mode!");
        break;
    }
}


void Well::CalWI_Peaceman(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    // this fomular needs to be carefully checked !
    // especially the dz

    for (USI p = 0; p < numPerf; p++) {
        if (perf[p].WI > 0) {
            break;
        } else {
            const OCP_USI Idb = perf[p].location;
            const OCP_DBL dx  = myBulk.dx[Idb];
            const OCP_DBL dy  = myBulk.dy[Idb];
            const OCP_DBL dz  = myBulk.dz[Idb] * myBulk.ntg[Idb];
            OCP_DBL       ro  = 0;
            switch (perf[p].direction) {
                case X_DIRECTION:
                    {
                        const OCP_DBL kykz  = myBulk.rockKy[Idb] * myBulk.rockKz[Idb];
                        const OCP_DBL ky_kz = myBulk.rockKy[Idb] / myBulk.rockKz[Idb];
                        assert(kykz > 0);
                        ro = 0.28 * pow((dy * dy * pow(1 / ky_kz, 0.5) +
                                         dz * dz * pow(ky_kz, 0.5)),
                                        0.5);
                        ro /= (pow(ky_kz, 0.25) + pow(1 / ky_kz, 0.25));

                        if (perf[p].kh < 0) {
                            perf[p].kh = (dx * pow(kykz, 0.5));
                        }
                        break;
                    }
                case Y_DIRECTION:
                    {
                        const OCP_DBL kzkx  = myBulk.rockKz[Idb] * myBulk.rockKx[Idb];
                        const OCP_DBL kz_kx = myBulk.rockKz[Idb] / myBulk.rockKx[Idb];
                        assert(kzkx > 0);
                        ro = 0.28 * pow((dz * dz * pow(1 / kz_kx, 0.5) +
                                         dx * dx * pow(kz_kx, 0.5)),
                                        0.5);
                        ro /= (pow(kz_kx, 0.25) + pow(1 / kz_kx, 0.25));

                        if (perf[p].kh < 0) {
                            perf[p].kh = (dy * pow(kzkx, 0.5));
                        }
                        break;
                    }
                case Z_DIRECTION:
                    {
                        const OCP_DBL kxky  = myBulk.rockKx[Idb] * myBulk.rockKy[Idb];
                        const OCP_DBL kx_ky = myBulk.rockKx[Idb] / myBulk.rockKy[Idb];
                        assert(kxky > 0);
                        ro = 0.28 * pow((dx * dx * pow(1 / kx_ky, 0.5) +
                                         dy * dy * pow(kx_ky, 0.5)),
                                        0.5);
                        ro /= (pow(kx_ky, 0.25) + pow(1 / kx_ky, 0.25));

                        if (perf[p].kh < 0) {
                            perf[p].kh = (dz * pow(kxky, 0.5));
                        }
                        break;
                    }
                default:
                    OCP_ABORT("Wrong direction of perforations!");
            }
            perf[p].WI = CONV2 * (2 * PI) * perf[p].kh /
                         (log(ro / perf[p].radius) + perf[p].skinFactor);
        }
    }
}

void Well::CalTrans(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    if (opt.type == INJ) {
        for (USI p = 0; p < numPerf; p++) {
            perf[p].transINJ = 0;
            OCP_USI k        = perf[p].location;
            OCP_DBL temp     = CONV1 * perf[p].WI * perf[p].multiplier;

            // single phase
            for (USI j = 0; j < np; j++) {
                perf[p].transj[j] = 0;
                OCP_USI id        = k * np + j;
                if (myBulk.phaseExist[id]) {
                    perf[p].transj[j] = temp * myBulk.kr[id] / myBulk.mu[id];
                    perf[p].transINJ += perf[p].transj[j];
                }
            }
            if (ifUseUnweight) {
                perf[p].transINJ = perf[p].WI;
            }
        }
    } else {
        for (USI p = 0; p < numPerf; p++) {
            OCP_USI k    = perf[p].location;
            OCP_DBL temp = CONV1 * perf[p].WI * perf[p].multiplier;

            // multi phase
            for (USI j = 0; j < np; j++) {
                perf[p].transj[j] = 0;
                OCP_USI id        = k * np + j;
                if (myBulk.phaseExist[id]) {
                    perf[p].transj[j] = temp * myBulk.kr[id] / myBulk.mu[id];
                }
            }
        }
    }
}

void Well::CalFlux(const Bulk& myBulk, const OCP_BOOL ReCalXi)
{
    OCP_FUNCNAME;

    // cout << name << endl;
    fill(qi_lbmol.begin(), qi_lbmol.end(), 0.0);

    if (opt.type == INJ) {

        for (USI p = 0; p < numPerf; p++) {
            perf[p].P  = bhp + dG[p];
            OCP_USI k  = perf[p].location;
            OCP_DBL dP = myBulk.P[k] - perf[p].P;

            perf[p].qt_ft3 = perf[p].transINJ * dP;

            if (ReCalXi) {
                USI pvtnum = myBulk.PVTNUM[k];
                perf[p].xi = myBulk.flashCal[pvtnum]->XiPhase(
                    perf[p].P, opt.injTemp, opt.injZi, opt.injPhase);
            }
            for (USI i = 0; i < nc; i++) {
                perf[p].qi_lbmol[i] = perf[p].qt_ft3 * perf[p].xi * opt.injZi[i];
                qi_lbmol[i] += perf[p].qi_lbmol[i];
            }
        }
    } else {

        for (USI p = 0; p < numPerf; p++) {
            perf[p].P      = bhp + dG[p];
            OCP_USI k      = perf[p].location;
            perf[p].qt_ft3 = 0;
            fill(perf[p].qi_lbmol.begin(), perf[p].qi_lbmol.end(), 0.0);
            fill(perf[p].qj_ft3.begin(), perf[p].qj_ft3.end(), 0.0);

            for (USI j = 0; j < np; j++) {
                OCP_USI id = k * np + j;
                if (myBulk.phaseExist[id]) {
                    OCP_DBL dP = myBulk.Pj[id] - perf[p].P;

                    perf[p].qj_ft3[j] = perf[p].transj[j] * dP;
                    perf[p].qt_ft3 += perf[p].qj_ft3[j];

                    OCP_DBL xi = myBulk.xi[id];
                    OCP_DBL xij;
                    for (USI i = 0; i < nc; i++) {
                        xij = myBulk.xij[id * nc + i];
                        perf[p].qi_lbmol[i] += perf[p].qj_ft3[j] * xi * xij;
                    }
                }
            }
            for (USI i = 0; i < nc; i++) qi_lbmol[i] += perf[p].qi_lbmol[i];
        }
    }
}

/// Pressure in injection well equals maximum ones in injection well,
/// which is input by users. this function is used to check if operation mode of
/// well shoubld be swtched.
OCP_DBL Well::CalInjRateMaxBHP(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    OCP_DBL qj    = 0;
    OCP_DBL Pwell = opt.maxBHP;

    for (USI p = 0; p < numPerf; p++) {

        OCP_DBL Pperf = Pwell + dG[p];
        OCP_USI k     = perf[p].location;

        USI     pvtnum = myBulk.PVTNUM[k];
        OCP_DBL xi = myBulk.flashCal[pvtnum]->XiPhase(Pperf, opt.injTemp, opt.injZi,
                                                      opt.injPhase);

        OCP_DBL dP = Pperf - myBulk.P[k];
        qj += perf[p].transINJ * xi * dP;
    }
    return qj;
}

/// Pressure in production well equals minial ones in production well,
/// which is input by users. this function is used to check if operation mode of
/// well shoubld be swtched.
OCP_DBL Well::CalProdRateMinBHP(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    OCP_DBL qj    = 0;
    OCP_DBL Pwell = opt.minBHP;

    vector<OCP_DBL> tmpQi_lbmol(nc, 0);
    vector<OCP_DBL> tmpQj(np, 0);

    for (USI p = 0; p < numPerf; p++) {

        OCP_DBL Pperf = Pwell + dG[p];
        OCP_USI k     = perf[p].location;

        for (USI j = 0; j < np; j++) {
            OCP_USI id = k * np + j;
            if (myBulk.phaseExist[id]) {
                OCP_DBL dP   = myBulk.Pj[id] - Pperf;
                OCP_DBL temp = perf[p].transj[j] * myBulk.xi[id] * dP;
                for (USI i = 0; i < nc; i++) {
                    tmpQi_lbmol[i] += myBulk.xij[id * nc + i] * temp;
                }
            }
        }
    }

    qj = 0;
    mixture->CalVStd(Psurf, Tsurf, &tmpQi_lbmol[0]);
    for (USI j = 0; j < np; j++) {
        qj += mixture->GetVarSet().vj[j] / unitConvert[j] * opt.prodPhaseWeight[j];
    }

    return qj;
}

void Well::CalInjQj(const Bulk& myBulk, const OCP_DBL& dt)
{
    OCP_FUNCNAME;

    OCP_DBL qj = 0;

    for (USI i = 0; i < nc; i++) {
        qj += qi_lbmol[i];
    }
    if (opt.fluidType == "WAT") {
        WWIR = -qj / opt.factorINJ;
        WWIT += WWIR * dt;
    } else {
        WGIR = -qj / opt.factorINJ; // Mscf or lbmol -> Mscf
        WGIT += WGIR * dt;
    }
}

void Well::CalProdQj(const Bulk& myBulk, const OCP_DBL& dt)
{
    mixture->CalVStd(Psurf, Tsurf, &qi_lbmol[0]);
    const auto o = mixture->OilIndex();
    const auto g = mixture->GasIndex();
    const auto w = mixture->WatIndex();

    if (o >= 0) WOPR = mixture->GetVarSet().vj[o] / unitConvert[o];
    if (g >= 0) WGPR = mixture->GetVarSet().vj[g] / unitConvert[g];
    if (w >= 0) WWPR = mixture->GetVarSet().vj[w] / unitConvert[w];

    WOPT += WOPR * dt;
    WGPT += WGPR * dt;
    WWPT += WWPR * dt;
}

/// It calculates pressure difference between perforations iteratively.
/// This function can be used in both black oil model and compositional model.
/// stability of this method shoule be tested.
void Well::CaldG(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    if (opt.type == INJ)
        CalInjdG(myBulk);
    else
        CalProddG01(myBulk);
}

void Well::CalInjdG(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    const OCP_DBL   maxlen  = 10;
    USI             seg_num = 0;
    OCP_DBL         seg_len = 0;
    vector<OCP_DBL> dGperf(numPerf, 0);

    if (depth <= perf.front().depth) {
        // Well is higher
        for (OCP_INT p = numPerf - 1; p >= 0; p--) {
            if (p == 0) {
                seg_num = ceil(fabs((perf[0].depth - depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[0].depth - depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p].depth - perf[p - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p].depth - perf[p - 1].depth) / seg_num;
            }
            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            USI pvtnum = myBulk.PVTNUM[n];
            for (USI i = 0; i < seg_num; i++) {
                Ptmp -= myBulk.flashCal[pvtnum]->RhoPhase(
                            Ptmp, 0, opt.injTemp, opt.injZi, opt.injPhase) *
                        GRAVITY_FACTOR * seg_len;
            }
            dGperf[p] = Pperf - Ptmp;
        }
        dG[0] = dGperf[0];
        for (USI p = 1; p < numPerf; p++) {
            dG[p] = dG[p - 1] + dGperf[p];
        }
    } else if (depth >= perf[numPerf - 1].depth) {
        // Well is lower
        for (USI p = 0; p < numPerf; p++) {
            if (p == numPerf - 1) {
                seg_num = ceil(fabs((depth - perf[numPerf - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (depth - perf[numPerf - 1].depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p + 1].depth - perf[p].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p + 1].depth - perf[p].depth) / seg_num;
            }
            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            USI pvtnum = myBulk.PVTNUM[n];
            for (USI i = 0; i < seg_num; i++) {
                Ptmp += myBulk.flashCal[pvtnum]->RhoPhase(
                            Ptmp, 0, opt.injTemp, opt.injZi, opt.injPhase) *
                        GRAVITY_FACTOR * seg_len;
            }
            dGperf[p] = Ptmp - Pperf;
        }
        dG[numPerf - 1] = dGperf[numPerf - 1];
        for (OCP_INT p = numPerf - 2; p >= 0; p--) {
            dG[p] = dG[p + 1] + dGperf[p];
        }
    }
}

// Use transj
void Well::CalProddG01(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    const OCP_DBL   maxlen  = 10;
    USI             seg_num = 0;
    OCP_DBL         seg_len = 0;
    vector<OCP_DBL> dGperf(numPerf, 0);
    vector<OCP_DBL> tmpNi(nc, 0);
    OCP_DBL         rhotmp, qtacc, rhoacc;

    if (depth <= perf.front().depth) {
        // Well is higher
        for (OCP_INT p = numPerf - 1; p >= 0; p--) {
            if (p == 0) {
                seg_num = ceil(fabs((perf[0].depth - depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[0].depth - depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p].depth - perf[p - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p].depth - perf[p - 1].depth) / seg_num;
            }
            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            // fill(tmpNi.begin(), tmpNi.end(), 0.0);
            for (USI j = 0; j < np; j++) {
                const OCP_USI n_np_j = n * np + j;
                if (!myBulk.phaseExist[n_np_j]) continue;
                for (USI k = 0; k < nc; k++) {
                    tmpNi[k] += (myBulk.P[n] - perf[p].P) * perf[p].transj[j] *
                                myBulk.xi[n_np_j] * myBulk.xij[n_np_j * nc + k];
                }
            }
            OCP_DBL tmpSum = Dnorm1(nc, &tmpNi[0]);
            if (tmpSum < TINY) {
                for (USI i = 0; i < nc; i++) {
                    tmpNi[i] = myBulk.Ni[n * nc + i];
                }
            }

            USI pvtnum = myBulk.PVTNUM[n];
            for (USI i = 0; i < seg_num; i++) {
                qtacc = rhoacc = 0;
                myBulk.flashCal[pvtnum]->Flash(Ptmp, myBulk.T[n], tmpNi.data());
                for (USI j = 0; j < np; j++) {
                    if (myBulk.flashCal[pvtnum]->GetPhaseExist(j)) {
                        rhotmp = myBulk.flashCal[pvtnum]->GetRho(j);
                        qtacc += myBulk.flashCal[pvtnum]->GetVj(j);
                        rhoacc += myBulk.flashCal[pvtnum]->GetVj(j) * rhotmp;
                    }
                }
                Ptmp -= rhoacc / qtacc * GRAVITY_FACTOR * seg_len;
            }
            dGperf[p] = Pperf - Ptmp;
        }
        dG[0] = dGperf[0];
        for (USI p = 1; p < numPerf; p++) {
            dG[p] = dG[p - 1] + dGperf[p];
        }
    } else if (depth >= perf[numPerf - 1].depth) {
        // Well is lower
        for (USI p = 0; p < numPerf; p++) {
            if (p == numPerf - 1) {
                seg_num = ceil(fabs((depth - perf[numPerf - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (depth - perf[numPerf - 1].depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p + 1].depth - perf[p].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p + 1].depth - perf[p].depth) / seg_num;
            }
            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            // fill(tmpNi.begin(), tmpNi.end(), 0.0);
            for (USI j = 0; j < np; j++) {
                const OCP_USI n_np_j = n * np + j;
                if (!myBulk.phaseExist[n_np_j]) continue;
                for (USI k = 0; k < nc; k++) {
                    tmpNi[k] += (myBulk.P[n] - perf[p].P) * perf[p].transj[j] *
                                myBulk.xi[n_np_j] * myBulk.xij[n_np_j * nc + k];
                }
            }
            OCP_DBL tmpSum = Dnorm1(nc, &tmpNi[0]);
            if (tmpSum < TINY) {
                for (USI i = 0; i < nc; i++) {
                    tmpNi[i] = myBulk.Ni[n * nc + i];
                }
            }

            USI pvtnum = myBulk.PVTNUM[n];
            for (USI i = 0; i < seg_num; i++) {
                qtacc = rhoacc = 0;
                myBulk.flashCal[pvtnum]->Flash(Ptmp, myBulk.T[n], tmpNi.data());
                for (USI j = 0; j < np; j++) {
                    if (myBulk.flashCal[pvtnum]->GetPhaseExist(j)) {
                        rhotmp = myBulk.flashCal[pvtnum]->GetRho(j);
                        qtacc += myBulk.flashCal[pvtnum]->GetVj(j);
                        rhoacc += myBulk.flashCal[pvtnum]->GetVj(j) * rhotmp;
                    }
                }
                Ptmp += rhoacc / qtacc * GRAVITY_FACTOR * seg_len;
            }
            dGperf[p] = Ptmp - Pperf;
        }
        dG[numPerf - 1] = dGperf[numPerf - 1];
        for (OCP_INT p = numPerf - 2; p >= 0; p--) {
            dG[p] = dG[p + 1] + dGperf[p];
        }
    }
}

// Use bulk
void Well::CalProddG02(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    const OCP_DBL   maxlen  = 10;
    USI             seg_num = 0;
    OCP_DBL         seg_len = 0;
    vector<OCP_DBL> dGperf(numPerf, 0);
    vector<OCP_DBL> tmpNi(nc, 0);
    OCP_DBL         rhotmp, qtacc, rhoacc;

    if (depth <= perf.front().depth) {
        // Well is higher
        for (OCP_INT p = numPerf - 1; p >= 0; p--) {
            if (p == 0) {
                seg_num = ceil(fabs((perf[0].depth - depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[0].depth - depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p].depth - perf[p - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p].depth - perf[p - 1].depth) / seg_num;
            }
            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            fill(tmpNi.begin(), tmpNi.end(), 0.0);
            for (USI j = 0; j < np; j++) {
                const OCP_USI n_np_j = n * np + j;
                if (!myBulk.phaseExist[n_np_j]) continue;
                for (USI k = 0; k < nc; k++) {
                    tmpNi[k] += (perf[p].transj[j] > 0) * myBulk.xi[n_np_j] *
                                myBulk.xij[n_np_j * nc + k];
                }
            }
            OCP_DBL tmpSum = Dnorm1(nc, &tmpNi[0]);
            if (tmpSum < TINY) {
                for (USI i = 0; i < nc; i++) {
                    tmpNi[i] = myBulk.Ni[n * nc + i];
                }
            }

            USI pvtnum = myBulk.PVTNUM[n];
            for (USI i = 0; i < seg_num; i++) {
                qtacc = rhoacc = 0;
                myBulk.flashCal[pvtnum]->Flash(Ptmp, myBulk.T[n], tmpNi.data());
                for (USI j = 0; j < np; j++) {
                    if (myBulk.flashCal[pvtnum]->GetPhaseExist(j)) {
                        rhotmp = myBulk.flashCal[pvtnum]->GetRho(j);
                        qtacc += myBulk.flashCal[pvtnum]->GetVj(j);
                        rhoacc += myBulk.flashCal[pvtnum]->GetVj(j) * rhotmp;
                    }
                }
                Ptmp -= rhoacc / qtacc * GRAVITY_FACTOR * seg_len;
            }
            dGperf[p] = Pperf - Ptmp;
        }
        dG[0] = dGperf[0];
        for (USI p = 1; p < numPerf; p++) {
            dG[p] = dG[p - 1] + dGperf[p];
        }
    } else if (depth >= perf[numPerf - 1].depth) {
        // Well is lower
        for (USI p = 0; p < numPerf; p++) {
            if (p == numPerf - 1) {
                seg_num = ceil(fabs((depth - perf[numPerf - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (depth - perf[numPerf - 1].depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p + 1].depth - perf[p].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p + 1].depth - perf[p].depth) / seg_num;
            }
            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            fill(tmpNi.begin(), tmpNi.end(), 0.0);
            for (USI j = 0; j < np; j++) {
                const OCP_USI n_np_j = n * np + j;
                if (!myBulk.phaseExist[n_np_j]) continue;
                for (USI k = 0; k < nc; k++) {
                    tmpNi[k] += (perf[p].transj[j] > 0) * myBulk.xi[n_np_j] *
                                myBulk.xij[n_np_j * nc + k];
                }
            }
            OCP_DBL tmpSum = Dnorm1(nc, &tmpNi[0]);
            if (tmpSum < TINY) {
                for (USI i = 0; i < nc; i++) {
                    tmpNi[i] = myBulk.Ni[n * nc + i];
                }
            }

            USI pvtnum = myBulk.PVTNUM[n];
            for (USI i = 0; i < seg_num; i++) {
                qtacc = rhoacc = 0;
                myBulk.flashCal[pvtnum]->Flash(Ptmp, myBulk.T[n], tmpNi.data());
                for (USI j = 0; j < np; j++) {
                    if (myBulk.flashCal[pvtnum]->GetPhaseExist(j)) {
                        rhotmp = myBulk.flashCal[pvtnum]->GetRho(j);
                        qtacc += myBulk.flashCal[pvtnum]->GetVj(j);
                        rhoacc += myBulk.flashCal[pvtnum]->GetVj(j) * rhotmp;
                    }
                }
                Ptmp += rhoacc / qtacc * GRAVITY_FACTOR * seg_len;
            }
            dGperf[p] = Ptmp - Pperf;
        }
        dG[numPerf - 1] = dGperf[numPerf - 1];
        for (OCP_INT p = numPerf - 2; p >= 0; p--) {
            dG[p] = dG[p + 1] + dGperf[p];
        }
    }
}

// Use qi_lbmol
void Well::CalProddG(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    const OCP_DBL   maxlen  = 5;
    USI             seg_num = 0;
    OCP_DBL         seg_len = 0;
    vector<OCP_DBL> tmpNi(nc, 0);
    vector<OCP_DBL> dGperf(numPerf, 0);
    OCP_DBL         qtacc  = 0;
    OCP_DBL         rhoacc = 0;
    OCP_DBL         rhotmp = 0;

    if (depth <= perf.front().depth) {
        // Well is higher

        // check qi_lbmol   ----   test
        if (perf[numPerf - 1].state == CLOSE) {
            for (OCP_INT p = numPerf - 2; p >= 0; p--) {
                if (perf[p].state == OPEN) {
                    for (USI i = 0; i < nc; i++) {
                        perf[numPerf - 1].qi_lbmol[i] = perf[p].qi_lbmol[i];
                    }
                    break;
                }
            }
        }

        for (OCP_INT p = numPerf - 1; p >= 0; p--) {

            if (p == 0) {
                seg_num = ceil(fabs((perf[0].depth - depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[0].depth - depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p].depth - perf[p - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p].depth - perf[p - 1].depth) / seg_num;
            }

            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            USI pvtnum = myBulk.PVTNUM[n];
            // tmpNi.assign(nc, 0);
            // for (OCP_INT p1 = numPerf - 1; p1 >= p; p1--) {
            //    for (USI i = 0; i < nc; i++) {
            //        tmpNi[i] += perf[p1].qi_lbmol[i];
            //    }
            //}

            for (USI i = 0; i < nc; i++) {
                tmpNi[i] += perf[p].qi_lbmol[i];
            }

            // check tmpNi
            for (auto& v : tmpNi) {
                v = fabs(v);
            }

            for (USI k = 0; k < seg_num; k++) {
                myBulk.flashCal[pvtnum]->Flash(Ptmp, myBulk.T[n], tmpNi.data());
                for (USI j = 0; j < np; j++) {
                    if (myBulk.flashCal[pvtnum]->GetPhaseExist(j)) {
                        rhotmp = myBulk.flashCal[pvtnum]->GetRho(j);
                        qtacc += myBulk.flashCal[pvtnum]->GetVj(j) / seg_num;
                        rhoacc += myBulk.flashCal[pvtnum]->GetVj(j) * rhotmp *
                                  GRAVITY_FACTOR / seg_num;
#ifdef DEBUG
                        if (rhotmp <= 0 || !isfinite(rhotmp)) {
                            OCP_ABORT("Wrong rho " + to_string(rhotmp));
                        }
#endif // DEBUG
                    }
                }
                Ptmp -= rhoacc / qtacc * seg_len;
            }
            dGperf[p] = Pperf - Ptmp;
        }
        dG[0] = dGperf[0];
        for (USI p = 1; p < numPerf; p++) {
            dG[p] = dG[p - 1] + dGperf[p];
        }
    } else if (depth >= perf.back().depth) {
        // Well is lower

        // check qi_lbmol   ----   test
        if (perf[0].state == CLOSE) {
            for (USI p = 1; p <= numPerf; p++) {
                if (perf[p].state == OPEN) {
                    for (USI i = 0; i < nc; i++) {
                        perf[numPerf - 1].qi_lbmol[i] = perf[p].qi_lbmol[i];
                    }
                    break;
                }
            }
        }

        for (USI p = 0; p < numPerf; p++) {
            if (p == numPerf - 1) {
                seg_num = ceil(fabs((depth - perf[numPerf - 1].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (depth - perf[numPerf - 1].depth) / seg_num;
            } else {
                seg_num = ceil(fabs((perf[p + 1].depth - perf[p].depth) / maxlen));
                if (seg_num == 0) continue;
                seg_len = (perf[p + 1].depth - perf[p].depth) / seg_num;
            }

            OCP_USI n     = perf[p].location;
            perf[p].P     = bhp + dG[p];
            OCP_DBL Pperf = perf[p].P;
            OCP_DBL Ptmp  = Pperf;

            USI pvtnum = myBulk.PVTNUM[n];
            fill(tmpNi.begin(), tmpNi.end(), 0.0);
            for (OCP_INT p1 = numPerf - 1; p1 - p >= 0; p1--) {
                for (USI i = 0; i < nc; i++) {
                    tmpNi[i] += perf[p1].qi_lbmol[i];
                }
            }

            // check tmpNi
            for (auto& v : tmpNi) {
                v = fabs(v);
            }

            for (USI k = 0; k < seg_num; k++) {
                myBulk.flashCal[pvtnum]->Flash(Ptmp, myBulk.T[n], tmpNi.data());
                for (USI j = 0; j < np; j++) {
                    if (myBulk.flashCal[pvtnum]->GetPhaseExist(j)) {
                        rhotmp = myBulk.flashCal[pvtnum]->GetRho(j);
                        qtacc += myBulk.flashCal[pvtnum]->GetVj(j) / seg_num;
                        rhoacc += myBulk.flashCal[pvtnum]->GetVj(j) * rhotmp *
                                  GRAVITY_FACTOR / seg_num;
                    }
                }
                Ptmp += rhoacc / qtacc * seg_len;
            }
            dGperf[p] = Ptmp - Pperf;
        }
        dG[numPerf - 1] = dGperf[numPerf - 1];
        for (OCP_INT p = numPerf - 2; p >= 0; p--) {
            dG[p] = dG[p + 1] + dGperf[p];
        }

    } else {
        OCP_ABORT("Wrong well position!");
    }
}

void Well::CalFactor(const Bulk& myBulk) const
{
    if (opt.optMode == BHP_MODE)  return;

    if (opt.type == PROD) {
        if (mixture->IfBlkModel()) {
            // For black oil models -- phase = components
            vector<OCP_DBL> qitmp(nc, 1.0);
            mixture->CalVStd(Psurf, Tsurf, &qitmp[0]);
            for (USI i = 0; i < nc; i++) {
                factor[i] = mixture->GetVarSet().vj[i] / unitConvert[i] * opt.prodPhaseWeight[i];
            }
        }
        else {
            // For other models
            vector<OCP_DBL> qitmp(nc, 0);
            OCP_DBL         qt   = 0;
            OCP_BOOL        flag = OCP_TRUE;
            for (USI i = 0; i < nc; i++) {
                qt += qi_lbmol[i];
                if (qi_lbmol[i] < 0) flag = OCP_FALSE;
            }
            if (qt > TINY && flag) {
                qitmp = qi_lbmol;
            }
            else {
                for (USI p = 0; p < numPerf; p++) {
                    OCP_USI n = perf[p].location;

                    for (USI j = 0; j < np; j++) {
                        const OCP_USI n_np_j = n * np + j;
                        if (!myBulk.phaseExist[n_np_j]) continue;
                        for (USI k = 0; k < nc; k++) {
                            qitmp[k] += perf[p].transj[j] * myBulk.xi[n_np_j] *
                                myBulk.xij[n_np_j * nc + k];
                        }
                    }
                }
            }

            qt = 0;
            for (USI i = 0; i < nc; i++) qt += qitmp[i];
            OCP_DBL qv = 0;
            mixture->CalVStd(Psurf, Tsurf, &qitmp[0]);
            for (USI j = 0; j < np; j++) {
                qv += mixture->GetVarSet().vj[j] / unitConvert[j] * opt.prodPhaseWeight[j];
            }
            fill(factor.begin(), factor.end(), qv / qt);
            if (factor[0] < 1E-12) {
                OCP_ABORT("Wrong Condition!");
            }
        }
    }
    else if(opt.type == INJ) {
        //OCP_DBL fac = mixture->CalVmStd(Psurf, Tsurf, &opt.injZi[0], opt.injPhase);
        //if (opt.injPhase == PhaseType::oil)  fac *= unitConvert[mixture->OilIndex()];
        //if (opt.injPhase == PhaseType::gas)  fac *= unitConvert[mixture->GasIndex()];
        //if (opt.injPhase == PhaseType::wat)  fac *= unitConvert[mixture->WatIndex()];


    }
    else {
        OCP_ABORT("WRONG Well Type!");
    }
}

void Well::CalReInjFluid(const Bulk& myBulk, vector<OCP_DBL>& myZi)
{
    CalTrans(myBulk);

    for (USI p = 0; p < numPerf; p++) {
        OCP_USI n = perf[p].location;

        for (USI j = 0; j < np; j++) {
            const OCP_USI n_np_j = n * np + j;
            if (!myBulk.phaseExist[n_np_j]) continue;
            for (USI k = 0; k < nc; k++) {
                myZi[k] +=
                    perf[p].transj[j] * myBulk.xi[n_np_j] * myBulk.xij[n_np_j * nc + k];
            }
        }
    }
}

void Well::CorrectBHP()
{
    if (opt.type == PROD && opt.optMode == BHP_MODE) {
        bhp = opt.minBHP;
    } else if (opt.type == INJ && opt.optMode == BHP_MODE) {
        bhp = opt.maxBHP;
    }
}

/// Constant well pressure would be applied if flow rate is too large.
/// Constant flow rate would be applied if well pressure is outranged.
void Well::CheckOptMode(const Bulk& myBulk)
{
    OCP_FUNCNAME;
    if (opt.initOptMode == BHP_MODE) {
        if (opt.type == INJ) {
            OCP_DBL q = CalInjRateMaxBHP(myBulk);
            // for INJ well, maxRate has been switch to lbmols
            OCP_DBL tarRate = opt.maxRate;
            if (opt.reInj) {
                if (opt.reInjPhase == GAS)
                    tarRate = WGIR;
                else if (opt.reInjPhase == WATER)
                    tarRate = WWIR;
            }
            if (q > tarRate) {
                opt.optMode = RATE_MODE;
            } else {
                opt.optMode = BHP_MODE;
                bhp         = opt.maxBHP;
            }
        } else {
            opt.optMode = BHP_MODE;
            bhp         = opt.minBHP;
        }
    } else {
        if (opt.type == INJ) {
            OCP_DBL q = CalInjRateMaxBHP(myBulk);
            // for INJ well, maxRate has been switch to lbmols
            OCP_DBL tarRate = opt.maxRate;
            if (opt.reInj) {
                if (opt.reInjPhase == GAS)
                    tarRate = WGIR;
                else if (opt.reInjPhase == WATER)
                    tarRate = WWIR;
            }

            if (q > tarRate) {
                opt.optMode = opt.initOptMode;
            } else {
                opt.optMode = BHP_MODE;
                bhp         = opt.maxBHP;
            }
        } else {
            OCP_DBL q = CalProdRateMinBHP(myBulk);
            // cout << q << endl;
            if (q > opt.maxRate) {
                opt.optMode = opt.initOptMode;
            } else {
                opt.optMode = BHP_MODE;
                bhp         = opt.minBHP;
            }
        }
    }
}

OCP_INT Well::CheckP(const Bulk& myBulk)
{
    OCP_FUNCNAME;
    // 0 : all correct
    // 1 : negative P
    // 2 : outlimited P
    // 3 : crossflow happens

    if (bhp < 0) {
        cout << "### WARNING: Well " << name << " BHP = " << bhp << endl;
        return WELL_NEGATIVE_PRESSURE;
    }
    for (USI p = 0; p < numPerf; p++) {
        if (perf[p].state == OPEN && perf[p].P < 0) {
#ifdef DEBUG
            cout << "### WARNING: Well " << name << " Perf[" << p
                 << "].P = " << perf[p].P << endl;
#endif // DEBUG
            return WELL_NEGATIVE_PRESSURE;
        }
    }

    if (opt.type == INJ) {
        if (opt.optMode != BHP_MODE && bhp > opt.maxBHP) {
#if _DEBUG
            cout << "### WARNING: Well " << name << " switch to BHPMode" << endl;
#endif
            opt.optMode = BHP_MODE;
            bhp         = opt.maxBHP;
            return WELL_SWITCH_TO_BHPMODE;
        }
    } else {
        if (opt.optMode != BHP_MODE && bhp < opt.minBHP) {
#if _DEBUG
            cout << "### WARNING: Well " << name << " switch to BHPMode" << endl;
#endif
            opt.optMode = BHP_MODE;
            bhp         = opt.minBHP;
            return WELL_SWITCH_TO_BHPMODE;
        }
    }

    return CheckCrossFlow(myBulk);
}

OCP_INT Well::CheckCrossFlow(const Bulk& myBulk)
{
    OCP_FUNCNAME;

    OCP_USI  k;
    OCP_BOOL flagC = OCP_TRUE;

    if (opt.type == PROD) {
        for (USI p = 0; p < numPerf; p++) {
            k            = perf[p].location;
            OCP_DBL minP = myBulk.P[k];
            if (perf[p].state == OPEN && minP < perf[p].P) {
                cout << std::left << std::setw(12) << name << "  "
                     << "Well P = " << perf[p].P << ", "
                     << "Bulk P = " << minP << endl;
                perf[p].state      = CLOSE;
                perf[p].multiplier = 0;
                flagC              = OCP_FALSE;
                break;
            } else if (perf[p].state == CLOSE && minP > perf[p].P) {
                perf[p].state      = OPEN;
                perf[p].multiplier = 1;
            }
        }
    } else {
        for (USI p = 0; p < numPerf; p++) {
            k = perf[p].location;
            if (perf[p].state == OPEN && myBulk.P[k] > perf[p].P) {
                cout << std::left << std::setw(12) << name << "  "
                     << "Well P = " << perf[p].P << ", "
                     << "Bulk P = " << myBulk.P[k] << endl;
                perf[p].state      = CLOSE;
                perf[p].multiplier = 0;
                flagC              = OCP_FALSE;
                break;
            } else if (perf[p].state == CLOSE && myBulk.P[k] < perf[p].P) {
                perf[p].state      = OPEN;
                perf[p].multiplier = 1;
            }
        }
    }

    OCP_BOOL flag = OCP_FALSE;
    // check well --  if all perf are closed, open the depthest perf
    for (USI p = 0; p < numPerf; p++) {
        if (perf[p].state == OPEN) {
            flag = OCP_TRUE;
            break;
        }
    }

    if (!flag) {
        // open the deepest perf
        perf.back().state      = OPEN;
        perf.back().multiplier = 1;
        cout << "### WARNING: All perfs of " << name
             << " are closed! Open the last perf!\n";
    }

    if (!flagC) {
        // if crossflow happens, then corresponding perforation will be closed,
        // the multiplier of perforation will be set to zero, so trans of well
        // should be recalculated!
        //
        // dG = ldG;
        CalTrans(myBulk);
        // CalFlux(myBulk);
        // CaldG(myBulk);
        // CheckOptMode(myBulk);
        return WELL_CROSSFLOW;
    }

    return WELL_SUCCESS;
}

void Well::ShowPerfStatus(const Bulk& myBulk) const
{
    OCP_FUNCNAME;

    cout << fixed;
    cout << "----------------------------" << endl;
    cout << name << ":    " << opt.optMode << "   " << setprecision(3) << bhp << endl;
    for (USI p = 0; p < numPerf; p++) {
        vector<OCP_DBL> Qitmp(perf[p].qi_lbmol);
        // OCP_DBL         qt = Dnorm1(nc, &Qitmp[0]);
        OCP_USI n = perf[p].location;
        cout << setw(3) << p << "   " << perf[p].state << "   " << setw(6)
             << perf[p].location << "  " << setw(2) << perf[p].I + 1 << "  " << setw(2)
             << perf[p].J + 1 << "  " << setw(2) << perf[p].K + 1 << "  " << setw(10)
             << setprecision(6) << perf[p].WI << "  "               // ccf
             << setprecision(3) << perf[p].radius << "  "           // ccf
             << setw(8) << setprecision(4) << perf[p].kh << "  "    // kh
             << setw(8) << setprecision(2) << perf[p].depth << "  " // depth
             << setprecision(3) << perf[p].P << "  "                // Pp
             << setw(10) << setprecision(3) << myBulk.P[n] << "   " // Pb
             << setw(6) << setprecision(3) << dG[p] << "   "        // dG
             << setw(8) << perf[p].qi_lbmol[nc - 1] << "   " << setw(6)
             << setprecision(6) << myBulk.S[n * np + 0] << "   " << setw(6)
             << setprecision(6) << myBulk.S[n * np + 1] << "   " << setw(6)
             << setprecision(6) << myBulk.S[n * np + 2] << endl;
    }
}


void Well::CalResFIM(OCP_USI& wId, OCPRes& res, const Bulk& bk, const OCP_DBL& dt) const
{
    if (opt.state == OPEN) {
        const USI len = nc + 1;
        // Well to Bulk
        for (USI p = 0; p < numPerf; p++) {
            const OCP_USI k = perf[p].location;
            for (USI i = 0; i < nc; i++) {
                res.resAbs[k * len + 1 + i] += perf[p].qi_lbmol[i] * dt;
            }
        }
        // Well Self
        if (opt.type == INJ) {
            // Injection
            switch (opt.optMode) {
            case BHP_MODE:
                res.resAbs[wId] = bhp - opt.maxBHP;
                break;
            case RATE_MODE:
            case ORATE_MODE:
            case GRATE_MODE:
            case WRATE_MODE:
            case LRATE_MODE:
                res.resAbs[wId] = opt.maxRate;
                for (USI i = 0; i < nc; i++) {
                    res.resAbs[wId] += qi_lbmol[i];
                }
                // if (opt.reInj) {
                //     for (auto& w : opt.connWell) {
                //         OCP_DBL tmp = 0;
                //         for (USI i = 0; i < nc; i++) {
                //             tmp += allWell[w].qi_lbmol[i];
                //         }
                //         tmp *= opt.reInjFactor;
                //         Res.resAbs[wId] += tmp;
                //     }
                // }
                res.maxWellRelRes_mol =
                    max(res.maxWellRelRes_mol,
                        fabs(res.resAbs[wId] / opt.maxRate));
                break;
            default:
                OCP_ABORT("Wrong well opt mode!");
                break;
            }
        }
        else {
            // Production
            switch (opt.optMode) {
            case BHP_MODE:
                res.resAbs[wId] = bhp - opt.minBHP;
                break;
            case RATE_MODE:
            case ORATE_MODE:
            case GRATE_MODE:
            case WRATE_MODE:
            case LRATE_MODE:
                CalFactor(bk);
                res.resAbs[wId] = -opt.maxRate;
                for (USI i = 0; i < nc; i++) {
                    res.resAbs[wId] += qi_lbmol[i] * factor[i];
                }
                res.maxWellRelRes_mol =
                    max(res.maxWellRelRes_mol,
                        fabs(res.resAbs[wId] / opt.maxRate));
                break;
            default:
                OCP_ABORT("Wrong well opt mode!");
                break;
            }
        }
        wId += len;
    }
}


void Well::AssembleMatFIM(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    if (opt.state == OPEN) {
        if (opt.type == INJ)        AssembleMatInjFIM(ls, bk, dt);
        else if (opt.type == PROD)  AssembleMatProdFIM(ls, bk, dt);
        else                        OCP_ABORT("WRONG Well Type!");
    }
}


void Well::AssembleMatInjFIM(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const 
{
    const USI ncol   = nc + 1;
    const USI ncol2  = np * nc + np;
    const USI bsize  = ncol * ncol;
    const USI bsize2 = ncol * ncol2;
    OCP_USI   n_np_j;

    vector<OCP_DBL> bmat(bsize, 0);
    vector<OCP_DBL> bmat2(bsize, 0);
    vector<OCP_DBL> dQdXpB(bsize, 0);
    vector<OCP_DBL> dQdXpW(bsize, 0);
    vector<OCP_DBL> dQdXsB(bsize2, 0);

    OCP_DBL mu, muP, dP, transIJ;

    const OCP_USI wId = ls.AddDim(1) - 1;
    ls.NewDiag(wId, bmat);

    for (USI p = 0; p < numPerf; p++) {
        const OCP_USI n = perf[p].location;
        fill(dQdXpB.begin(), dQdXpB.end(), 0.0);
        fill(dQdXpW.begin(), dQdXpW.end(), 0.0);
        fill(dQdXsB.begin(), dQdXsB.end(), 0.0);

        dP = bk.P[n] - bhp - dG[p];

        for (USI j = 0; j < np; j++) {
            n_np_j = n * np + j;
            if (!bk.phaseExist[n_np_j]) continue;

            mu = bk.mu[n_np_j];
            muP = bk.muP[n_np_j];

            for (USI i = 0; i < nc; i++) {
                // dQ / dP
                transIJ = perf[p].transj[j] * perf[p].xi * opt.injZi[i];
                dQdXpB[(i + 1) * ncol] += transIJ * (1 - dP * muP / mu);
                dQdXpW[(i + 1) * ncol] += -transIJ;

                // dQ / dS
                for (USI k = 0; k < np; k++) {
                    dQdXsB[(i + 1) * ncol2 + k] +=
                        CONV1 * perf[p].WI * perf[p].multiplier * perf[p].xi *
                        opt.injZi[i] * bk.dKrdS[n_np_j * np + k] * dP / mu;
                }
                // dQ / dxij
                for (USI k = 0; k < nc; k++) {
                    dQdXsB[(i + 1) * ncol2 + np + j * nc + k] +=
                        -transIJ * dP / mu * bk.mux[n_np_j * nc + k];
                }
            }
        }

        // Bulk to Well
        bmat = dQdXpB;
        DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2], 1,
            bmat.data());
        Dscalar(bsize, dt, bmat.data());
        // Bulk - Bulk -- add
        ls.AddDiag(n, bmat);

        // Bulk - Well -- insert
        bmat = dQdXpW;
        Dscalar(bsize, dt, bmat.data());
        ls.NewOffDiag(n, wId, bmat);

        // Well
        switch (opt.optMode) {
        case RATE_MODE:
        case ORATE_MODE:
        case GRATE_MODE:
        case WRATE_MODE:
        case LRATE_MODE:
            // Well - Well -- add
            fill(bmat.begin(), bmat.end(), 0.0);
            for (USI i = 0; i < nc; i++) {
                bmat[0] += dQdXpW[(i + 1) * ncol];
                bmat[(i + 1) * ncol + i + 1] = 1;
            }
            ls.AddDiag(wId, bmat);

            // Well - Bulk -- insert
            bmat = dQdXpB;
            DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2],
                1, bmat.data());
            fill(bmat2.begin(), bmat2.end(), 0.0);
            for (USI i = 0; i < nc; i++) {
                Daxpy(ncol, 1.0, bmat.data() + (i + 1) * ncol, bmat2.data());
            }
            ls.NewOffDiag(wId, n, bmat2);
            break;

        case BHP_MODE:
            // Well - Well -- add
            fill(bmat.begin(), bmat.end(), 0.0);
            for (USI i = 0; i < ncol; i++) {
                bmat[i * ncol + i] = 1;
            }
            ls.AddDiag(wId, bmat);

            // Well - Bulk -- insert
            fill(bmat.begin(), bmat.end(), 0.0);
            ls.NewOffDiag(wId, n, bmat);
            break;

        default:
            OCP_ABORT("Wrong Well Opt mode!");
            break;
        }
    }
}


void Well::AssembleMatProdFIM(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    const USI ncol   = nc + 1;
    const USI ncol2  = np * nc + np;
    const USI bsize  = ncol * ncol;
    const USI bsize2 = ncol * ncol2;
    OCP_USI   n_np_j;

    vector<OCP_DBL> bmat(bsize, 0);
    vector<OCP_DBL> bmat2(bsize, 0);
    vector<OCP_DBL> dQdXpB(bsize, 0);
    vector<OCP_DBL> dQdXpW(bsize, 0);
    vector<OCP_DBL> dQdXsB(bsize2, 0);

    OCP_DBL xij, xi, mu, muP, xiP, dP, transIJ, tmp;

    const OCP_USI wId = ls.AddDim(1) - 1;
    ls.NewDiag(wId, bmat);

    // Set Commponent Weight
    CalFactor(bk);

    for (USI p = 0; p < numPerf; p++) {
        const OCP_USI n = perf[p].location;
        fill(dQdXpB.begin(), dQdXpB.end(), 0.0);
        fill(dQdXpW.begin(), dQdXpW.end(), 0.0);
        fill(dQdXsB.begin(), dQdXsB.end(), 0.0);

        for (USI j = 0; j < np; j++) {
            n_np_j = n * np + j;
            if (!bk.phaseExist[n_np_j]) continue;

            dP  = bk.Pj[n_np_j] - bhp - dG[p];
            xi  = bk.xi[n_np_j];
            mu  = bk.mu[n_np_j];
            muP = bk.muP[n_np_j];
            xiP = bk.xiP[n_np_j];

            for (USI i = 0; i < nc; i++) {
                xij = bk.xij[n_np_j * nc + i];
                // dQ / dP
                transIJ = perf[p].transj[j] * xi * xij;
                dQdXpB[(i + 1) * ncol] += transIJ * (1 - dP * muP / mu) +
                                          dP * perf[p].transj[j] * xij * xiP;
                dQdXpW[(i + 1) * ncol] += -transIJ;

                // dQ / dS
                for (USI k = 0; k < np; k++) {
                    tmp = CONV1 * perf[p].WI * perf[p].multiplier * dP / mu * xi *
                          xij * bk.dKrdS[n_np_j * np + k];
                    // capillary pressure
                    tmp += transIJ * bk.dPcdS[n_np_j * np + k];
                    dQdXsB[(i + 1) * ncol2 + k] += tmp;
                }
                // dQ / dCij
                for (USI k = 0; k < nc; k++) {
                    tmp = dP * perf[p].transj[j] * xij *
                          (bk.xix[n_np_j * nc + k] - xi / mu * bk.mux[n_np_j * nc + k]);
                    dQdXsB[(i + 1) * ncol2 + np + j * nc + k] += tmp;
                }
                dQdXsB[(i + 1) * ncol2 + np + j * nc + i] +=
                    perf[p].transj[j] * xi * dP;
            }
        }

        // Bulk - Bulk -- add
        bmat = dQdXpB;
        DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2], 1,
                bmat.data());

        Dscalar(bsize, dt, bmat.data());
        ls.AddDiag(n, bmat);

        // Bulk - Well -- insert
        bmat = dQdXpW;
        Dscalar(bsize, dt, bmat.data());
        ls.NewOffDiag(n, wId, bmat);

        // Well
        switch (opt.optMode) {
            case RATE_MODE:
            case ORATE_MODE:
            case GRATE_MODE:
            case WRATE_MODE:
            case LRATE_MODE:
                // Well - Well -- add
                fill(bmat.begin(), bmat.end(), 0.0);
                for (USI i = 0; i < nc; i++) {
                    bmat[0] += dQdXpW[(i + 1) * ncol] * factor[i];
                    bmat[(i + 1) * ncol + i + 1] = 1;
                }
                ls.AddDiag(wId, bmat);

                // Well - Bulk -- insert
                bmat = dQdXpB;
                DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2],
                        1, bmat.data());
                fill(bmat2.begin(), bmat2.end(), 0.0);
                for (USI i = 0; i < nc; i++) {
                    Daxpy(ncol, factor[i], bmat.data() + (i + 1) * ncol,
                          bmat2.data());
                }
                ls.NewOffDiag(wId, n, bmat2);
                break;

            case BHP_MODE:
                // Well - Well -- add
                fill(bmat.begin(), bmat.end(), 0.0);
                for (USI i = 0; i < ncol; i++) {
                    bmat[i * ncol + i] = 1;
                }
                ls.AddDiag(wId, bmat);

                // Well - Bulk -- insert
                fill(bmat.begin(), bmat.end(), 0.0);
                ls.NewOffDiag(wId, n, bmat);
                break;

            default:
                OCP_ABORT("Wrong Well Opt mode!");
                break;
        }
    }
}


void Well::AssembleMatIMPEC(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    if (opt.state == OPEN) {
        if (opt.type == INJ)        AssembleMatInjIMPEC(ls, bk, dt);
        else if (opt.type == PROD)  AssembleMatProdIMPEC(ls, bk, dt);
        else                        OCP_ABORT("WRONG Well Type!");
    }
}


void Well::AssembleMatInjIMPEC(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    const OCP_USI wId = ls.AddDim(1) - 1;
    ls.NewDiag(wId, 0.0);

    OCP_DBL Vfi_zi, valb, valw, bb, bw;

    for (USI p = 0; p < numPerf; p++) {
        const OCP_USI n = perf[p].location;

        Vfi_zi = 0;
        for (USI i = 0; i < nc; i++) {
            Vfi_zi += bk.vfi[n * nc + i] * opt.injZi[i];
        }

        valw = dt * perf[p].xi * perf[p].transINJ;
        bw   = valw * dG[p];
        valb = valw * Vfi_zi;
        bb   = valb * dG[p];

        // Bulk to Well
        ls.AddDiag(n, valb);
        ls.NewOffDiag(n, wId, -valb);
        ls.AddRhs(n, bb);

        // Well to Bulk
        switch (opt.optMode) {
            case RATE_MODE:
            case ORATE_MODE:
            case GRATE_MODE:
            case WRATE_MODE:
            case LRATE_MODE:
                ls.AddDiag(wId, valw);
                ls.NewOffDiag(wId, n, -valw);
                ls.AddRhs(wId, -bw);
                break;
            case BHP_MODE:
                ls.NewOffDiag(wId, n, 0);
                break;
            default:
                OCP_ABORT("Wrong well option mode!");
        }
    }

    // Well Self
    switch (opt.optMode) {
        case RATE_MODE:
        case ORATE_MODE:
        case GRATE_MODE:
        case WRATE_MODE:
        case LRATE_MODE:
            ls.AddRhs(wId, dt * opt.maxRate);
            break;
        case BHP_MODE:
            ls.AddDiag(wId, dt);
            ls.AddRhs(wId, dt * opt.maxBHP);
            ls.AssignGuess(wId, opt.maxBHP);
            break;
        default:
            OCP_ABORT("Wrong well option mode!");
    }
}


void Well::AssembleMatProdIMPEC(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    const OCP_USI wId = ls.AddDim(1) - 1;
    ls.NewDiag(wId, 0.0);

    // Set Prod Weight
    CalFactor(bk);

    for (USI p = 0; p < numPerf; p++) {
        const OCP_USI n = perf[p].location;

        OCP_DBL valb = 0;
        OCP_DBL bb   = 0;
        OCP_DBL valw = 0;
        OCP_DBL bw   = 0;

        for (USI j = 0; j < np; j++) {
            const OCP_USI n_np_j = n * np + j;
            if (!bk.phaseExist[n_np_j]) continue;

            OCP_DBL tempb = 0;
            OCP_DBL tempw = 0;

            for (USI i = 0; i < nc; i++) {
                tempb += bk.vfi[n * nc + i] * bk.xij[n_np_j * nc + i];
                tempw += factor[i] * bk.xij[n_np_j * nc + i];
            }
            OCP_DBL trans = dt * perf[p].transj[j] * bk.xi[n_np_j];
            valb += tempb * trans;
            valw += tempw * trans;

            OCP_DBL dP = dG[p] - bk.Pc[n_np_j];
            bb += tempb * trans * dP;
            bw += tempw * trans * dP;
        }

        // Bulk to Well
        ls.AddDiag(n, valb);
        ls.NewOffDiag(n, wId, -valb);
        ls.AddRhs(n, bb);

        // Well to Bulk
        switch (opt.optMode) {
            case RATE_MODE:
            case ORATE_MODE:
            case GRATE_MODE:
            case WRATE_MODE:
            case LRATE_MODE:
                ls.AddDiag(wId, -valw);
                ls.NewOffDiag(wId, n, valw);
                ls.AddRhs(wId, bw);
                break;
            case BHP_MODE:
                ls.NewOffDiag(wId, n, 0.0);
                break;
            default:
                OCP_ABORT("Wrong well option mode!");
        }
    }

    // Well Self
    switch (opt.optMode) {
        case RATE_MODE:
        case ORATE_MODE:
        case GRATE_MODE:
        case WRATE_MODE:
        case LRATE_MODE:
            ls.AddRhs(wId, dt * opt.maxRate);
            break;
        case BHP_MODE:
            ls.AddDiag(wId, dt);
            ls.AddRhs(wId, dt * opt.minBHP);
            ls.AssignGuess(wId, opt.minBHP);
            break;
        default:
            OCP_ABORT("Wrong well option mode!");
    }
}


void Well::CalResFIM_T(OCP_USI& wId, OCPRes& res, const Bulk& bk, const OCP_DBL& dt) const
{
	if (opt.state == OPEN) {
        const USI len = nc + 2;

		// Well to Bulk
		for (USI p = 0; p < numPerf; p++) {
			const OCP_USI k = perf[p].location;
			// Mass Conservation
			for (USI i = 0; i < nc; i++) {
				res.resAbs[k * len + 1 + i] += perf[p].qi_lbmol[i] * dt;
			}
		}

		if (opt.type == INJ) {
			// Energy Conservation -- Well to Bulk
			const OCP_DBL Hw =
				bk.flashCal[0]->CalInjWellEnthalpy(opt.injTemp, &opt.injZi[0]);
			for (USI p = 0; p < numPerf; p++) {
				const OCP_USI k = perf[p].location;
				res.resAbs[k * len + 1 + nc] += perf[p].qt_ft3 * perf[p].xi * Hw * dt;
			}

			// Well Self --  Injection
			switch (opt.optMode) {
			case BHP_MODE:
				res.resAbs[wId] = bhp - opt.maxBHP;
				break;
			case RATE_MODE:
			case ORATE_MODE:
			case GRATE_MODE:
			case WRATE_MODE:
			case LRATE_MODE:
				res.resAbs[wId] = opt.maxRate;
				for (USI i = 0; i < nc; i++) {
					res.resAbs[wId] += qi_lbmol[i];
				}
				// if (opt.reInj) {
				//     for (auto& w : opt.connWell) {
				//         OCP_DBL tmp = 0;
				//         for (USI i = 0; i < nc; i++) {
				//             tmp += allWell[w].qi_lbmol[i];
				//         }
				//         tmp *= opt.reInjFactor;
				//         res.resAbs[bId] += tmp;
				//     }
				// }
				res.maxWellRelRes_mol =
					max(res.maxWellRelRes_mol,
						fabs(res.resAbs[wId] / opt.maxRate));
				break;
			default:
				OCP_ABORT("Wrong well opt mode!");
				break;
			}
		}
		else {
			// Energy Conservation -- Bulk to Well
			for (USI p = 0; p < numPerf; p++) {
				const OCP_USI k = perf[p].location;
				// Mass Conservation
				for (USI j = 0; j < np; j++) {
					res.resAbs[k * len + 1 + nc] += perf[p].qj_ft3[j] *
						bk.xi[k * np + j] * bk.H[k * np + j] * dt;
				}
			}

			// Well Self --  Production
			switch (opt.optMode) {
			case BHP_MODE:
				res.resAbs[wId] = bhp - opt.minBHP;
				break;
			case RATE_MODE:
			case ORATE_MODE:
			case GRATE_MODE:
			case WRATE_MODE:
			case LRATE_MODE:
				CalFactor(bk);
				res.resAbs[wId] = -opt.maxRate;
				for (USI i = 0; i < nc; i++) {
					res.resAbs[wId] += qi_lbmol[i] * factor[i];
				}
				res.maxWellRelRes_mol =
					max(res.maxWellRelRes_mol,
						fabs(res.resAbs[wId] / opt.maxRate));
				break;
			default:
				OCP_ABORT("Wrong well opt mode!");
				break;
			}
		}
		wId += len;
	}
}


void Well::AssembleMatFIM_T(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    if (opt.state == OPEN) {
        if (opt.type == INJ)        AssembleMatInjFIM_T(ls, bk, dt);
        else if (opt.type == PROD)  AssembleMatProdFIM_T(ls, bk, dt);
        else                        OCP_ABORT("WRONG Well Type!");
    }
}


void Well::AssembleMatInjFIM_T(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    const USI ncol   = nc + 2;
    const USI ncol2  = np * nc + np;
    const USI bsize  = ncol * ncol;
    const USI bsize2 = ncol * ncol2;
    OCP_USI   n_np_j;

    vector<OCP_DBL> bmat(bsize, 0);
    vector<OCP_DBL> bmat2(bsize, 0);
    vector<OCP_DBL> dQdXpB(bsize, 0);
    vector<OCP_DBL> dQdXpW(bsize, 0);
    vector<OCP_DBL> dQdXsB(bsize2, 0);

    OCP_DBL mu, muP, muT, dP, Hw;
    OCP_DBL transJ, transIJ;

    const OCP_USI wId = ls.AddDim(1) - 1;
    ls.NewDiag(wId, bmat);

    Hw = bk.flashCal[0]->CalInjWellEnthalpy(opt.injTemp, &opt.injZi[0]);

    for (USI p = 0; p < numPerf; p++) {
        const OCP_USI n = perf[p].location;
        fill(dQdXpB.begin(), dQdXpB.end(), 0.0);
        fill(dQdXpW.begin(), dQdXpW.end(), 0.0);
        fill(dQdXsB.begin(), dQdXsB.end(), 0.0);

        dP = bk.P[n] - bhp - dG[p];

        for (USI j = 0; j < np; j++) {
            n_np_j = n * np + j;
            if (!bk.phaseExist[n_np_j]) continue;

            mu  = bk.mu[n_np_j];
            muP = bk.muP[n_np_j];
            muT = bk.muT[n_np_j];

            for (USI i = 0; i < nc; i++) {

                // Mass Conservation
                if (!ifUseUnweight) {
                    transIJ = perf[p].transj[j] * perf[p].xi * opt.injZi[i];
                    // dQ / dP
                    dQdXpB[(i + 1) * ncol] += transIJ * (1 - dP * muP / mu);
                    dQdXpW[(i + 1) * ncol] += -transIJ;

                    // dQ / dT
                    dQdXpB[(i + 2) * ncol - 1] += transIJ * (-dP * muT / mu);
                    dQdXpW[(i + 2) * ncol - 1] += 0;
                } else {
                    // dQ / dP
                    transIJ = perf[p].transINJ * perf[p].xi * opt.injZi[i];
                    dQdXpB[(i + 1) * ncol] += transIJ;
                    dQdXpW[(i + 1) * ncol] += -transIJ;
                }

                if (!ifUseUnweight) {
                    // dQ / dS
                    for (USI k = 0; k < np; k++) {
                        dQdXsB[(i + 1) * ncol2 + k] +=
                            CONV1 * perf[p].WI * perf[p].multiplier * perf[p].xi *
                            opt.injZi[i] * bk.dKrdS[n_np_j * np + k] * dP / mu;
                    }
                    // dQ / dxij
                    for (USI k = 0; k < nc; k++) {
                        dQdXsB[(i + 1) * ncol2 + np + j * nc + k] +=
                            -transIJ * dP / mu * bk.mux[n_np_j * nc + k];
                    }
                }
            }

            // Energy Conservation
            if (!ifUseUnweight) {
                transJ = perf[p].transj[j] * perf[p].xi;
                // dQ / dP
                dQdXpB[(nc + 1) * ncol] += transJ * Hw * (1 - dP * muP / mu);
                dQdXpW[(nc + 1) * ncol] += -transJ * Hw;

                // dQ / dT
                dQdXpB[(nc + 2) * ncol - 1] += transJ * Hw * (-dP * muT / mu);
                dQdXpW[(nc + 2) * ncol - 1] += 0;

                // dQ / dS
                for (USI k = 0; k < np; k++) {
                    dQdXsB[(nc + 1) * ncol2 + k] +=
                        CONV1 * perf[p].WI * perf[p].multiplier * perf[p].xi *
                        bk.dKrdS[n_np_j * np + k] * dP / mu * Hw;
                }
                // dQ / dxij
                for (USI k = 0; k < nc; k++) {
                    dQdXsB[(nc + 1) * ncol2 + np + j * nc + k] +=
                        -transJ * dP / mu * bk.mux[n_np_j * nc + k] * Hw;
                }
            } else {
                transJ = perf[p].transINJ * perf[p].xi;
                // dQ / dP
                dQdXpB[(nc + 1) * ncol] += transJ * Hw;
                dQdXpW[(nc + 1) * ncol] += -transJ * Hw;
            }

            if (ifUseUnweight) break;
        }

        // Bulk to Well
        bmat = dQdXpB;
        DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2], 1,
                bmat.data());
        Dscalar(bsize, dt, bmat.data());
        // Add
        ls.AddDiag(n, bmat);

        // Insert
        bmat = dQdXpW;
        Dscalar(bsize, dt, bmat.data());
        ls.NewOffDiag(n, wId, bmat);

        // Well
        switch (opt.optMode) {
            case RATE_MODE:
            case ORATE_MODE:
            case GRATE_MODE:
            case WRATE_MODE:
            case LRATE_MODE:
                // Diag
                fill(bmat.begin(), bmat.end(), 0.0);
                for (USI i = 0; i < nc; i++) {
                    bmat[0] += dQdXpW[(i + 1) * ncol];
                    bmat[(i + 1) * ncol + i + 1] = 1;
                }
                bmat[ncol * ncol - 1] = 1;
                ls.AddDiag(wId, bmat);

                // OffDiag
                bmat = dQdXpB;
                DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2],
                        1, bmat.data());
                fill(bmat2.begin(), bmat2.end(), 0.0);
                for (USI i = 0; i < nc; i++) {
                    Daxpy(ncol, 1.0, bmat.data() + (i + 1) * ncol, bmat2.data());
                }
                ls.NewOffDiag(wId, n, bmat2);
                break;

            case BHP_MODE:
                // Diag
                fill(bmat.begin(), bmat.end(), 0.0);
                for (USI i = 0; i < ncol; i++) {
                    bmat[i * ncol + i] = 1;
                }
                // Add
                ls.AddDiag(wId, bmat);
                // OffDiag
                fill(bmat.begin(), bmat.end(), 0.0);
                // Insert
                ls.NewOffDiag(wId, n, bmat);
                break;

            default:
                OCP_ABORT("Wrong Well Opt mode!");
                break;
        }
    }
}


void Well::AssembleMatProdFIM_T(LinearSystem& ls, const Bulk& bk, const OCP_DBL& dt) const
{
    const USI ncol   = nc + 2;
    const USI ncol2  = np * nc + np;
    const USI bsize  = ncol * ncol;
    const USI bsize2 = ncol * ncol2;
    OCP_USI   n_np_j;

    vector<OCP_DBL> bmat(bsize, 0);
    vector<OCP_DBL> bmat2(bsize, 0);
    vector<OCP_DBL> dQdXpB(bsize, 0);
    vector<OCP_DBL> dQdXpW(bsize, 0);
    vector<OCP_DBL> dQdXsB(bsize2, 0);

    OCP_DBL xij, xi, xiP, xiT, mu, muP, muT, dP, transIJ, transJ, H, HT, Hx, tmp;

    const OCP_USI wId = ls.AddDim(1) - 1;
    ls.NewDiag(wId, bmat);

    // Set Prod Weight
    CalFactor(bk);

    for (USI p = 0; p < numPerf; p++) {
        const OCP_USI n = perf[p].location;
        fill(dQdXpB.begin(), dQdXpB.end(), 0.0);
        fill(dQdXpW.begin(), dQdXpW.end(), 0.0);
        fill(dQdXsB.begin(), dQdXsB.end(), 0.0);

        for (USI j = 0; j < np; j++) {
            n_np_j = n * np + j;
            if (!bk.phaseExist[n_np_j]) continue;

            dP  = bk.Pj[n_np_j] - bhp - dG[p];
            xi  = bk.xi[n_np_j];
            mu  = bk.mu[n_np_j];
            xiP = bk.xiP[n_np_j];
            xiT = bk.xiT[n_np_j];
            muP = bk.muP[n_np_j];
            muT = bk.muT[n_np_j];
            H   = bk.H[n_np_j];
            HT  = bk.HT[n_np_j];

            // Mass Conservation
            for (USI i = 0; i < nc; i++) {
                xij = bk.xij[n_np_j * nc + i];
                Hx  = bk.Hx[n_np_j * nc + i];
                // dQ / dP
                transIJ = perf[p].transj[j] * xi * xij;
                dQdXpB[(i + 1) * ncol] += transIJ * (1 - dP * muP / mu) +
                                          dP * perf[p].transj[j] * xij * xiP;
                dQdXpW[(i + 1) * ncol] += -transIJ;

                // dQ / dT
                dQdXpB[(i + 2) * ncol - 1] +=
                    transIJ * (-dP * muT / mu) + dP * perf[p].transj[j] * xij * xiT;
                dQdXpW[(i + 2) * ncol - 1] += 0;

                // dQ / dS
                for (USI k = 0; k < np; k++) {
                    tmp = CONV1 * perf[p].WI * perf[p].multiplier * dP / mu * xi *
                          xij * bk.dKrdS[n_np_j * np + k];
                    // capillary pressure
                    tmp += transIJ * bk.dPcdS[n_np_j * np + k];
                    dQdXsB[(i + 1) * ncol2 + k] += tmp;
                }
                // dQ / dxij
                for (USI k = 0; k < nc; k++) {
                    tmp = dP * perf[p].transj[j] * xij *
                          (bk.xix[n_np_j * nc + k] - xi / mu * bk.mux[n_np_j * nc + k]);
                    dQdXsB[(i + 1) * ncol2 + np + j * nc + k] += tmp;
                }
                dQdXsB[(i + 1) * ncol2 + np + j * nc + i] +=
                    perf[p].transj[j] * xi * dP;
            }

            // Energy Conservation
            transJ = perf[p].transj[j] * xi;
            // dQ / dP
            dQdXpB[(nc + 1) * ncol] +=
                transJ * (1 - dP * muP / mu) * H + dP * perf[p].transj[j] * xiP * H;
            dQdXpW[(nc + 1) * ncol] += -transJ * H;

            // dQ / dT
            dQdXpB[(nc + 2) * ncol - 1] += transJ * (-dP * muT / mu) * H +
                                           dP * perf[p].transj[j] * xiT * H +
                                           transJ * dP * HT;
            dQdXpW[(nc + 2) * ncol - 1] += 0;

            // dQ / dS
            for (USI k = 0; k < np; k++) {
                tmp = CONV1 * perf[p].WI * perf[p].multiplier * dP / mu * xi *
                      bk.dKrdS[n_np_j * np + k] * H;
                // capillary pressure
                tmp += transJ * bk.dPcdS[n_np_j * np + k] * H;
                dQdXsB[(nc + 1) * ncol2 + k] += tmp;
            }

            // dQ / dxij
            for (USI k = 0; k < nc; k++) {
                tmp = dP * perf[p].transj[j] *
                        (bk.xix[n_np_j * nc + k] - xi / mu * bk.mux[n_np_j * nc + k]) *
                        H +
                    transJ * dP * Hx;
                dQdXsB[(nc + 1) * ncol2 + np + j * nc + k] += tmp;
            }
        }

        // Bulk to Well
        bmat = dQdXpB;
        DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2], 1,
                bmat.data());
        Dscalar(bsize, dt, bmat.data());
        // Add
        ls.AddDiag(n, bmat);
        // Insert
        bmat = dQdXpW;
        Dscalar(bsize, dt, bmat.data());
        ls.NewOffDiag(n, wId, bmat);

        // Well
        switch (opt.optMode) {
            case RATE_MODE:
            case ORATE_MODE:
            case GRATE_MODE:
            case WRATE_MODE:
            case LRATE_MODE:
                // Diag
                fill(bmat.begin(), bmat.end(), 0.0);
                for (USI i = 0; i < nc; i++) {
                    bmat[0] += dQdXpW[(i + 1) * ncol] * factor[i];
                    bmat[(i + 1) * ncol + i + 1] = 1;
                }
                bmat[ncol * ncol - 1] = 1;
                ls.AddDiag(wId, bmat);

                // OffDiag
                bmat = dQdXpB;
                DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &bk.dSec_dPri[n * bsize2],
                        1, bmat.data());
                fill(bmat2.begin(), bmat2.end(), 0.0);
                for (USI i = 0; i < nc; i++) {
                    Daxpy(ncol, factor[i], bmat.data() + (i + 1) * ncol,
                          bmat2.data());
                }
                ls.NewOffDiag(wId, n, bmat2);
                break;

            case BHP_MODE:
                // Diag
                fill(bmat.begin(), bmat.end(), 0.0);
                for (USI i = 0; i < ncol; i++) {
                    bmat[i * ncol + i] = 1;
                }
                // Add
                ls.AddDiag(wId, bmat);
                // OffDiag
                fill(bmat.begin(), bmat.end(), 0.0);
                // Insert
                ls.NewOffDiag(wId, n, bmat);
                break;

            default:
                OCP_ABORT("Wrong Well Opt mode!");
                break;
        }
    }
}


//void Well::AssembleMatReinjection_IMPEC(const Bulk&         myBulk,
//                                        LinearSystem&       myLS,
//                                        const OCP_DBL&      dt,
//                                        const vector<Well>& allWell,
//                                        const vector<USI>&  injId) const
//{
//    // find Open injection well under Rate control
//    vector<OCP_USI> tarId;
//    for (auto& w : injId) {
//        if (allWell[w].IsOpen() && allWell[w].opt.optMode != BHP_MODE)
//            tarId.push_back(allWell[w].wOId + myBulk.numBulk);
//    }
//
//    USI tlen = tarId.size();
//    if (tlen > 0) {
//        // All inj well has the same factor
//        const OCP_DBL factor = allWell[injId[0]].opt.reInjFactor * dt;
//        const OCP_USI prodId = wOId + myBulk.numBulk;
//        OCP_USI       n, bId;
//        OCP_DBL       tmp, valb;
//        OCP_DBL       valw = 0;
//        OCP_DBL       rhsw = 0;
//        for (USI p = 0; p < numPerf; p++) {
//            n    = perf[p].location;
//            valb = 0;
//
//            for (USI j = 0; j < np; j++) {
//                bId = n * np + j;
//                if (myBulk.phaseExist[bId]) {
//                    tmp = perf[p].transj[j] * myBulk.xi[bId];
//                    valb += tmp;
//                    rhsw += tmp * (myBulk.Pc[bId] - dG[p]);
//                }
//            }
//            valb *= factor;
//            for (USI t = 0; t < tlen; t++) {
//                myLS.NewOffDiag(tarId[t], n, -valb);
//            }
//            valw += valb;
//        }
//        rhsw *= factor;
//        // rhs and prod well
//        for (USI t = 0; t < tlen; t++) {
//            myLS.AddRhs(tarId[t], rhsw);
//            myLS.NewOffDiag(tarId[t], prodId, valw);
//        }
//    }
//}
//
//void Well::AssembleMatReinjection_FIM(const Bulk&         myBulk,
//                                      LinearSystem&       myLS,
//                                      const OCP_DBL&      dt,
//                                      const vector<Well>& allWell,
//                                      const vector<USI>&  injId) const
//{
//    // find Open injection well under Rate control
//    vector<OCP_USI> tarId;
//    for (auto& w : injId) {
//        if (allWell[w].IsOpen() && allWell[w].opt.optMode != BHP_MODE)
//            tarId.push_back(allWell[w].wOId + myBulk.numBulk);
//    }
//
//    USI tlen = tarId.size();
//    if (tlen > 0) {
//        // All inj well has the same factor
//        const OCP_DBL factor = allWell[injId[0]].opt.reInjFactor;
//        const OCP_USI prodId = wOId + myBulk.numBulk;
//
//        const USI ncol   = nc + 1;
//        const USI ncol2  = np * nc + np;
//        const USI bsize  = ncol * ncol;
//        const USI bsize2 = ncol * ncol2;
//
//        OCP_DBL xij, xi, mu, muP, xiP, dP, transIJ, tmp;
//        OCP_USI n_np_j;
//
//        vector<OCP_DBL> bmat(bsize, 0);
//        vector<OCP_DBL> bmat2(bsize, 0);
//        vector<OCP_DBL> tmpMat(bsize, 0);
//        vector<OCP_DBL> dQdXpB(bsize, 0);
//        vector<OCP_DBL> dQdXpW(bsize, 0);
//        vector<OCP_DBL> dQdXsB(bsize2, 0);
//
//        for (USI p = 0; p < numPerf; p++) {
//            const OCP_USI n = perf[p].location;
//            fill(dQdXpB.begin(), dQdXpB.end(), 0.0);
//            fill(dQdXpW.begin(), dQdXpW.end(), 0.0);
//            fill(dQdXsB.begin(), dQdXsB.end(), 0.0);
//
//            for (USI j = 0; j < np; j++) {
//                n_np_j = n * np + j;
//                if (!myBulk.phaseExist[n_np_j]) continue;
//
//                dP  = myBulk.Pj[n_np_j] - bhp - dG[p];
//                xi  = myBulk.xi[n_np_j];
//                mu  = myBulk.mu[n_np_j];
//                muP = myBulk.muP[n_np_j];
//                xiP = myBulk.xiP[n_np_j];
//
//                for (USI i = 0; i < nc; i++) {
//                    xij = myBulk.xij[n_np_j * nc + i];
//                    // dQ / dP
//                    transIJ = perf[p].transj[j] * xi * xij;
//                    dQdXpB[(i + 1) * ncol] += transIJ * (1 - dP * muP / mu) +
//                                              dP * perf[p].transj[j] * xij * xiP;
//                    dQdXpW[(i + 1) * ncol] += -transIJ;
//
//                    // dQ / dS
//                    for (USI k = 0; k < np; k++) {
//                        tmp = CONV1 * perf[p].WI * perf[p].multiplier * dP / mu * xi *
//                              xij * myBulk.dKrdS[n_np_j * np + k];
//                        // capillary pressure
//                        tmp += transIJ * myBulk.dPcdS[n_np_j * np + k];
//                        dQdXsB[(i + 1) * ncol2 + k] += tmp;
//                    }
//                    // dQ / dCij
//                    for (USI k = 0; k < nc; k++) {
//                        tmp = dP * perf[p].transj[j] * xij *
//                              (myBulk.xix[n_np_j * nc + k] -
//                               xi / mu * myBulk.mux[n_np_j * nc + k]);
//                        if (k == i) {
//                            tmp += perf[p].transj[j] * xi * dP;
//                        }
//                        dQdXsB[(i + 1) * ncol2 + np + j * nc + k] += tmp;
//                    }
//                }
//            }
//
//            // for Prod Well
//            for (USI i = 0; i < nc; i++) {
//                tmpMat[0] += dQdXpW[(i + 1) * ncol] * factor;
//            }
//
//            // for Perf(bulk) of Prod Well
//            bmat = dQdXpB;
//            DaABpbC(ncol, ncol, ncol2, 1, dQdXsB.data(), &myBulk.dSec_dPri[n * bsize2],
//                    1, bmat.data());
//            fill(bmat2.begin(), bmat2.end(), 0.0);
//            for (USI i = 0; i < nc; i++) {
//                // becareful '-' before factor
//                Daxpy(ncol, -factor, bmat.data() + (i + 1) * ncol, bmat2.data());
//            }
//
//            // Insert bulk val into equations in ascending order
//            for (USI t = 0; t < tlen; t++) {
//                myLS.NewOffDiag(tarId[t], n, bmat2);
//            }
//        }
//        // prod well
//        for (USI t = 0; t < tlen; t++) {
//            myLS.NewOffDiag(tarId[t], prodId, tmpMat);
//        }
//    }
//}

//void Well::SetPolyhedronWell(const Grid& myGrid, OCPpolyhedron& mypol)
//{
//    // set a virtual point
//    mypol.numPoints = numPerf + 1;
//    mypol.Points.resize(mypol.numPoints);
//
//    OCP_USI k;
//    Point3D tmpP;
//
//    for (USI p = 0; p < numPerf; p++) {
//        tmpP.Reset();
//        k = myGrid.map_Act2All[perf[p].location];
//        for (USI i = 0; i < myGrid.polyhedronGrid[k].numPoints; i++) {
//            tmpP += myGrid.polyhedronGrid[k].Points[i];
//        }
//        tmpP /= myGrid.polyhedronGrid[k].numPoints;
//        mypol.Points[p + 1] = tmpP;
//    }
//
//    // Set virtual perf
//    mypol.Points[0] = mypol.Points[1];
//    mypol.Points[0].z /= 2;
//}

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/15/2021      Format file                          */
/*----------------------------------------------------------------------------*/
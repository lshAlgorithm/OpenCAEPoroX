/*! \file    MixtureUnit.hpp
 *  \brief   MixtureUnit class declaration
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __MIXTURE_HEADER__
#define __MIXTURE_HEADER__

// Standard header files
#include <iostream>
#include <vector>

// OpenCAEPoroX header files
#include "OCPConst.hpp"
#include "OptionalModules.hpp"
#include "ParamReservoir.hpp"
#include "OCPMixture.hpp"
#include "BulkVarSet.hpp"

using namespace std;

/// Mixture is an abstract class, who contains all information used for flash
/// calculation including variables, functions. any properties of phases such as mass
/// density can calculated by it. it has the same data structure as the ones in bulks.
class MixtureUnit
{
public:
    MixtureUnit(const ParamReservoir& rs_param, const USI& i, OptionalModules& opts);
    /// return type of mixture.
    auto GetMixtureType() const { return vs->mixtureType; }
    /// return the mixture.
    OCPMixture* GetMixture() const { return mix; }
    /// flash calculation
    void Flash(const OCP_DBL& Pin, const OCP_DBL& Tin, const OCP_DBL* Niin) const;
    /// flash calculation with saturation of phases for some bulk
    void InitFlashIMPEC(const OCP_USI& bId, const BulkVarSet& bvs) const;
    /// flash calculation with saturation of phases for some bulk
    void InitFlashFIM(const OCP_USI& bId, const BulkVarSet& bvs) const;
    /// Flash calculation with moles of components for some bulk
    void FlashIMPEC(const OCP_USI& bId, const BulkVarSet& bvs) const;
    /// Flash calculation with moles of components and Calculate the derivative for some bulk
    void FlashFIM(const OCP_USI& bId, const BulkVarSet& bvs) const;
    /// return mass density of phase
    // for blackoil model: if tarPhase is gas and water, Pin and tar phase is needed
    // for compositional model: if tar phase is hydrocarbon phase, Pin, Tin, Ziin is
    // needed. if tar phase is water, only Pin is needed.
    OCP_DBL XiPhase(const OCP_DBL& Pin, const OCP_DBL& Tin, const vector<OCP_DBL>& Ziin, const PhaseType& pt) const {
        return mix->CalXi(Pin, Pin, Tin, &Ziin[0], pt);
    }
    /// return mass density of phase
    // for blackoil model: if tarPhase is gas and water, Pin and tar phase is needed, if
    // tarPhase is oil,then Pbb is needed, too for compositional model: if tar phase is
    // hydrocarbon phase, Pin, Tin, Ziin is needed. if tar phase is water, only Pin is
    // needed.
    OCP_DBL RhoPhase(const OCP_DBL& Pin, const OCP_DBL& Pbb, const OCP_DBL& Tin,
                     const vector<OCP_DBL>& Ziin, const PhaseType& pt) const {
        return mix->CalRho(Pin, Pbb, Tin, &Ziin[0], pt);
    }
    // for well
    /// Calculate Production rate for PROD well
    OCP_DBL CalInjWellEnthalpy(const OCP_DBL& Tin, const OCP_DBL* Ziin) const {
        return mix->CalEnthalpy(Tin, Ziin);
    }
    void OutMixtureIters() const {
        mix->OutputIters();
    }

public:
    const auto GetVs() const { return vs; }
    const auto& GetNt() const { return vs->Nt; }
    const auto& GetNi(const USI& i) const { return vs->Ni[i]; }
    const auto& GetVf() const { return vs->Vf; }
    const auto& GetPhaseExist(const USI& j) const { return vs->phaseExist[j]; }
    const auto& GetS(const USI& j) const { return vs->S[j]; }
    const auto& GetVj(const USI& j) const { return vs->vj[j]; }
    const auto& GetNj(const USI& j) const { return vs->nj[j]; }
    const auto& GetXij(const USI& j, const USI& i) const { return vs->x[j * vs->nc + i]; }
    const auto& GetRho(const USI& j) const { return vs->rho[j]; }
    const auto& GetXi(const USI& j) const { return vs->xi[j]; }
    const auto& GetMu(const USI& j) const { return vs->mu[j]; }
    const auto& GetVfP() const { return vs->vfP; }
    const auto& GetVfT() const { return vs->vfT; }
    const auto& GetVfi(const USI& i) const { return vs->vfi[i]; }
    const auto& GetRhoP(const USI& j) const { return vs->rhoP[j]; }
    const auto& GetRhoT(const USI& j) const { return vs->rhoT[j]; }
    const auto& GetXiP(const USI& j) const { return vs->xiP[j]; }
    const auto& GetXiT(const USI& j) const { return vs->xiT[j]; }
    const auto& GetMuP(const USI& j) const { return vs->muP[j]; }
    const auto& GetMuT(const USI& j) const { return vs->muT[j]; }
    const auto& GetRhoX(const USI& j, const USI& i) const { return vs->rhox[j * vs->nc + i]; }
    const auto& GetXiX(const USI& j, const USI& i) const { return vs->xix[j * vs->nc + i]; }
    const auto& GetMuX(const USI& j, const USI& i) const { return vs->mux[j * vs->nc + i]; }
    const auto& GetDXsDXp() const { return vs->dXsdXp; }
    const auto& GetUf() const { return vs->Uf; }
    const auto& GetUfP() const { return vs->UfP; }
    const auto& GetUfT() const { return vs->UfT; }
    const auto& GetUfi(const USI& i) const { return vs->Ufi[i]; }
    const auto& GetH(const USI& j) const { return vs->H[j]; }
    const auto& GetHT(const USI& j) const { return vs->HT[j]; }
    const auto& GetHx(const USI& j, const USI& i) const { return vs->Hx[j * vs->nc + i]; }

protected:
    /// mixture of components
    OCPMixture*             mix;
    /// variable set for mixture
    const OCPMixtureVarSet* vs;

protected:
    /// Surface tension
    SurfaceTension* surTen;
    USI             stMethodIndex;
    /// Miscible Factor
    MiscibleFactor* misFac;
    USI             mfMethodIndex;
};

#endif /* end if __MIXTURE_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/15/2021      Format file                          */
/*----------------------------------------------------------------------------*/
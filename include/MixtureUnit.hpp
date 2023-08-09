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
#include "OptionalFeatures.hpp"
#include "ParamReservoir.hpp"
#include "WellOpt.hpp"
#include "OCPMixture.hpp"

using namespace std;

/// Mixture is an abstract class, who contains all information used for flash
/// calculation including variables, functions. any properties of phases such as mass
/// density can calculated by it. it has the same data structure as the ones in bulks.
class MixtureUnit
{
public:
    MixtureUnit() = default;
    /// return type of mixture.
    USI GetMixtureType() const { return mixtureType; }
    /// flash calculation with saturation of phases.
    virtual void Flash(const OCP_DBL& Pin, const OCP_DBL& Tin, const OCP_DBL* Niin) = 0;
    virtual void InitFlashIMPEC(const OCP_DBL& Pin,
                                const OCP_DBL& Pbbin,
                                const OCP_DBL& Tin,
                                const OCP_DBL* Sjin,
                                const OCP_DBL& Vpore,
                                const OCP_DBL* Ziin,
                                const OCP_USI& bId)                                 = 0;
    virtual void InitFlashFIM(const OCP_DBL& Pin,
                              const OCP_DBL& Pbbin,
                              const OCP_DBL& Tin,
                              const OCP_DBL* Sjin,
                              const OCP_DBL& Vpore,
                              const OCP_DBL* Ziin,
                              const OCP_USI& bId)                                   = 0;
    /// Flash calculation with moles of components.
    virtual void FlashIMPEC(const OCP_DBL& Pin,
                            const OCP_DBL& Tin,
                            const OCP_DBL* Niin,
                            const USI&     lastNP,
                            const OCP_DBL* xijin,
                            const OCP_USI& bId) = 0;
    /// Flash calculation with moles of components and Calculate the derivative
    virtual void FlashFIM(const OCP_DBL& Pin,
                          const OCP_DBL& Tin,
                          const OCP_DBL* Niin,
                          const OCP_DBL* Sjin,
                          const USI&     lastNP,
                          const OCP_DBL* xijin,
                          const OCP_USI& bId)  = 0;

    /// return mass density of phase
    // for blackoil model: if tarPhase is gas and water, Pin and tar phase is needed
    // for compositional model: if tar phase is hydrocarbon phase, Pin, Tin, Ziin is
    // needed. if tar phase is water, only Pin is needed.
    virtual OCP_DBL XiPhase(const OCP_DBL& Pin,
                            const OCP_DBL& Tin,
                            const vector<OCP_DBL>& Ziin,
                            const USI&     tarPhase) = 0;

    /// return mass density of phase
    // for blackoil model: if tarPhase is gas and water, Pin and tar phase is needed, if
    // tarPhase is oil,then Pbb is needed, too for compositional model: if tar phase is
    // hydrocarbon phase, Pin, Tin, Ziin is needed. if tar phase is water, only Pin is
    // needed.
    virtual OCP_DBL RhoPhase(const OCP_DBL& Pin,
                             const OCP_DBL& Pbb,
                             const OCP_DBL& Tin,
                             const vector<OCP_DBL>& Ziin,
                             const USI&     tarPhase) = 0;

    // for well
    /// Setup injZi, injPhase and factorINJ for INJ well
    /// Setup prodPhaseWeight for PROD well
    virtual void SetupWellOpt(WellOpt&                  opt,
                              const vector<SolventINJ>& sols,
                              const OCP_DBL&            Psurf,
                              const OCP_DBL&            Tsurf) = 0;
    /// Calculate ProdWeight for PROD well
    virtual void CalProdWeight(const OCP_DBL&         Pin,
                               const OCP_DBL&         Tin,
                               const OCP_DBL*         Niin,
                               const vector<OCP_DBL>& prodPhase,
                               vector<OCP_DBL>&       prodWeight) = 0;
    /// Calculate Production rate for PROD well
    virtual void    CalProdRate(const OCP_DBL&   Pin,
                                const OCP_DBL&   Tin,
                                const OCP_DBL*   Niin,
                                vector<OCP_DBL>& prodRate)                      = 0;
    virtual OCP_DBL CalInjWellEnthalpy(const OCP_DBL& Tin, const OCP_DBL* Ziin) = 0;

    virtual void OutMixtureIters() const = 0;

public:
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
    /// type of mixture model
    USI                     mixtureType;  
    const OCPMixtureVarSet* vs;
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
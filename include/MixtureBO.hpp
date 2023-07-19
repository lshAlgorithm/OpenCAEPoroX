/*! \file    MixtureBO.hpp
 *  \brief   MixtureBO class declaration
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __MIXTUREBO_HEADER__
#define __MIXTUREBO_HEADER__

#include <cmath>

// OpenCAEPoroX header files
#include "Mixture.hpp"
#include "OCPFuncPVT.hpp"
#include "OCPMixtureBlkOilOW.hpp"

/// BOMixture is inherited class of Mixture, it's used for black oil model.
class BOMixture : public Mixture
{
public:
    BOMixture() = default;
    void SetupOptionalFeatures(OptionalFeatures& optFeatures) override{};
    void BOMixtureInit(const ParamReservoir& rs_param);

    // For Well
    void CalProdWeight(const OCP_DBL&         Pin,
                       const OCP_DBL&         Tin,
                       const OCP_DBL*         Niin,
                       const vector<OCP_DBL>& prodPhase,
                       vector<OCP_DBL>&       prodWeight) override
    {
        prodWeight = prodPhase;
    }

    void CalProdRate(const OCP_DBL&   Pin,
                     const OCP_DBL&   Tin,
                     const OCP_DBL*   Niin,
                     vector<OCP_DBL>& prodRate) override
    {
        prodRate.assign(Niin, Niin + numCom);
    };
    OCP_DBL CalInjWellEnthalpy(const OCP_DBL& Tin, const OCP_DBL* Ziin) override
    {
        OCP_ABORT("Can not be used in Black Oil Model!");
    }

    OCP_DBL GetErrorPEC() override
    {
        OCP_ABORT("Should not be used in Black Oil mode!");
        return 0;
    }
    void OutMixtureIters() const override{};

protected:
    // USI mixtureType; ///< indicates the type of mixture, black oil or compositional
    // or
    ///< others.
    // std_Gamma* = std_Rho* * GRAVITY_FACTOR.
    // only one of rho and gamma is needed, the other will be calculated from it.
    OCP_DBL std_RhoO; ///< The density of oil at surface conditions : lb/ft3
    OCP_DBL std_RhoG; ///< The density of gas at surface conditions : lb/ft3
    OCP_DBL std_RhoW; ///< The density of water at surface conditions : lb/ft3

    vector<vector<OCP_DBL>> vji; ///< dVj/dNi
    vector<OCP_DBL>         vjP; ///< dVj/dP
};

///////////////////////////////////////////////
// BOMixture_W
///////////////////////////////////////////////

class BOMixture_W : public BOMixture
{
public:
    BOMixture_W() = default;
    BOMixture_W(const ParamReservoir& rs_param, const USI& i)
    {
        OCP_ABORT("Not Completed!");
    };
    void Flash(const OCP_DBL& Pin, const OCP_DBL& Tin, const OCP_DBL* Niin) override
    {
        OCP_ABORT("Not Completed!");
    }
    void InitFlashIMPEC(const OCP_DBL& Pin,
                        const OCP_DBL& Pbbin,
                        const OCP_DBL& Tin,
                        const OCP_DBL* Sjin,
                        const OCP_DBL& Vpore,
                        const OCP_DBL* Ziin,
                        const OCP_USI& bId) override
    {
        OCP_ABORT("Not Completed!");
    };
    void InitFlashFIM(const OCP_DBL& Pin,
                      const OCP_DBL& Pbbin,
                      const OCP_DBL& Tin,
                      const OCP_DBL* Sjin,
                      const OCP_DBL& Vpore,
                      const OCP_DBL* Ziin,
                      const OCP_USI& bId) override
    {
        OCP_ABORT("Not Completed!");
    };
    void FlashIMPEC(const OCP_DBL& Pin,
                    const OCP_DBL& Tin,
                    const OCP_DBL* Niin,
                    const USI&     lastNP,
                    const OCP_DBL* xijin,
                    const OCP_USI& bId) override
    {
        OCP_ABORT("Not Completed!");
    };
    void FlashFIM(const OCP_DBL& Pin,
                  const OCP_DBL& Tin,
                  const OCP_DBL* Niin,
                  const OCP_DBL* Sjin,
                  const USI&     lastNP,
                  const OCP_DBL* xijin,
                  const OCP_USI& bId) override
    {
        OCP_ABORT("Not Completed!");
    };
    OCP_DBL XiPhase(const OCP_DBL& Pin,
                    const OCP_DBL& Tin,
                    const OCP_DBL* Ziin,
                    const USI&     tarPhase) override
    {
        OCP_ABORT("Not Completed!");
        return 0;
    };
    OCP_DBL RhoPhase(const OCP_DBL& Pin,
                     const OCP_DBL& Pbb,
                     const OCP_DBL& Tin,
                     const OCP_DBL* Ziin,
                     const USI&     tarPhase) override
    {
        OCP_ABORT("Not Completed!");
        return 0;
    };

    // for Well
    void SetupWellOpt(WellOpt&                  opt,
                      const vector<SolventINJ>& sols,
                      const OCP_DBL&            Psurf,
                      const OCP_DBL&            Tsurf) override
    {
        OCP_ABORT("Not Completed!");
    };

private:
    OCP_PVTW PVTW;
};

///////////////////////////////////////////////
// BOMixture_OW
///////////////////////////////////////////////

class BOMixture_OW : public BOMixture
{
public:
    BOMixture_OW() = default;
    BOMixture_OW(const ParamReservoir& rs_param, const USI& i);
    void Flash(const OCP_DBL& Pin, const OCP_DBL& Tin, const OCP_DBL* Niin) override;
    void InitFlashIMPEC(const OCP_DBL& Pin,
                        const OCP_DBL& Pbbin,
                        const OCP_DBL& Tin,
                        const OCP_DBL* Sjin,
                        const OCP_DBL& Vpore,
                        const OCP_DBL* Ziin,
                        const OCP_USI& bId) override;
    void InitFlashFIM(const OCP_DBL& Pin,
                      const OCP_DBL& Pbbin,
                      const OCP_DBL& Tin,
                      const OCP_DBL* Sjin,
                      const OCP_DBL& Vpore,
                      const OCP_DBL* Ziin,
                      const OCP_USI& bId) override;
    void FlashIMPEC(const OCP_DBL& Pin,
                    const OCP_DBL& Tin,
                    const OCP_DBL* Niin,
                    const USI&     lastNP,
                    const OCP_DBL* xijin,
                    const OCP_USI& bId) override;
    void FlashFIM(const OCP_DBL& Pin,
                  const OCP_DBL& Tin,
                  const OCP_DBL* Niin,
                  const OCP_DBL* Sjin,
                  const USI&     lastNP,
                  const OCP_DBL* xijin,
                  const OCP_USI& bId) override;
    OCP_DBL XiPhase(const OCP_DBL& Pin,
                    const OCP_DBL& Tin,
                    const OCP_DBL* Ziin,
                    const USI&     tarPhase) override;
    OCP_DBL RhoPhase(const OCP_DBL& Pin,
                     const OCP_DBL& Pbb,
                     const OCP_DBL& Tin,
                     const OCP_DBL* Ziin,
                     const USI&     tarPhase) override;

    // for Well
    void SetupWellOpt(WellOpt&                  opt,
                      const vector<SolventINJ>& sols,
                      const OCP_DBL&            Psurf,
                      const OCP_DBL&            Tsurf) override;

    const OCP_DBL& GetNt() const override { return OWM.GetVarSet().Nt; }
    const OCP_DBL& GetNi(const USI& i) const override { 
        return OWM.GetVarSet().Ni[i];
    }
    const OCP_DBL& GetVf() const override { return OWM.GetVarSet().vf; }
    const OCP_BOOL& GetPhaseExist(const USI& j) const override { return OWM.GetVarSet().phaseExist[j]; }
    const OCP_DBL& GetS(const USI& j) const override { return OWM.GetVarSet().S[j]; }
    const OCP_DBL& GetVj(const USI& j) const override { return OWM.GetVarSet().vj[j]; }
    const OCP_DBL& GetNj(const USI& j) const override  { return OWM.GetVarSet().nj[j]; }
    const OCP_DBL& GetXij(const USI& j, const USI& i) const override
    {
        return OWM.GetVarSet().xij[j * numCom + i];
    }
    const OCP_DBL& GetRho(const USI& j) const override { return OWM.GetVarSet().rho[j]; }
    const OCP_DBL& GetXi(const USI& j) const override { return OWM.GetVarSet().xi[j]; }
    const OCP_DBL& GetMu(const USI& j) const override { return OWM.GetVarSet().mu[j]; }
    const OCP_DBL& GetVfP() const override { return OWM.GetVarSet().vfP; }
    const OCP_DBL& GetVfT() const override { return OWM.GetVarSet().vfT; }
    const OCP_DBL& GetVfi(const USI& i) const override { return OWM.GetVarSet().vfi[i]; }
    const OCP_DBL& GetRhoP(const USI& j) const override { return OWM.GetVarSet().rhoP[j]; }
    const OCP_DBL& GetRhoT(const USI& j) const override { return OWM.GetVarSet().rhoT[j]; }
    const OCP_DBL& GetXiP(const USI& j) const override { return OWM.GetVarSet().xiP[j]; }
    const OCP_DBL& GetXiT(const USI& j) const override { return OWM.GetVarSet().xiT[j]; }
    const OCP_DBL& GetMuP(const USI& j) const override { return OWM.GetVarSet().muP[j]; }
    const OCP_DBL& GetMuT(const USI& j) const override { return OWM.GetVarSet().muT[j]; }
    const OCP_DBL& GetRhoX(const USI& j, const USI& i) const override
    {
        return OWM.GetVarSet().rhox[j * numCom + i];
    }
    const OCP_DBL& GetXiX(const USI& j, const USI& i) const override
    {
        return OWM.GetVarSet().xix[j * numCom + i];
    }
    const OCP_DBL& GetMuX(const USI& j, const USI& i) const override
    {
        return OWM.GetVarSet().mux[j * numCom + i];
    }
    const vector<OCP_DBL>& GetDXsDXp() const override { return OWM.GetVarSet().dXsdXp; }

private:
    OCPMixtureBlkOilOW OWM;
};

///////////////////////////////////////////////
// BOMixture_ODGW
///////////////////////////////////////////////

class BOMixture_ODGW : public BOMixture
{
public:
    BOMixture_ODGW() = default;
    BOMixture_ODGW(const ParamReservoir& rs_param, const USI& i);

    void Flash(const OCP_DBL& Pin, const OCP_DBL& Tin, const OCP_DBL* Niin) override;
    void InitFlashIMPEC(const OCP_DBL& Pin,
                        const OCP_DBL& Pbbin,
                        const OCP_DBL& Tin,
                        const OCP_DBL* Sjin,
                        const OCP_DBL& Vpore,
                        const OCP_DBL* Ziin,
                        const OCP_USI& bId) override;
    void InitFlashFIM(const OCP_DBL& Pin,
                      const OCP_DBL& Pbbin,
                      const OCP_DBL& Tin,
                      const OCP_DBL* Sjin,
                      const OCP_DBL& Vpore,
                      const OCP_DBL* Ziin,
                      const OCP_USI& bId) override;
    void FlashIMPEC(const OCP_DBL& Pin,
                    const OCP_DBL& Tin,
                    const OCP_DBL* Niin,
                    const USI&     lastNP,
                    const OCP_DBL* xijin,
                    const OCP_USI& bId) override;
    void FlashFIM(const OCP_DBL& Pin,
                  const OCP_DBL& Tin,
                  const OCP_DBL* Niin,
                  const OCP_DBL* Sjin,
                  const USI&     lastNP,
                  const OCP_DBL* xijin,
                  const OCP_USI& bId) override;
    OCP_DBL XiPhase(const OCP_DBL& Pin,
                    const OCP_DBL& Tin,
                    const OCP_DBL* Ziin,
                    const USI&     tarPhase) override;
    OCP_DBL RhoPhase(const OCP_DBL& Pin,
                     const OCP_DBL& Pbb,
                     const OCP_DBL& Tin,
                     const OCP_DBL* Ziin,
                     const USI&     tarPhase) override;

    // for Well
    void SetupWellOpt(WellOpt&                  opt,
                      const vector<SolventINJ>& sols,
                      const OCP_DBL&            Psurf,
                      const OCP_DBL&            Tsurf) override;

protected:
    void CalNi(const OCP_DBL& Pin, const OCP_DBL& Pbbin, const OCP_DBL* Sjin, const OCP_DBL& Vpore);

protected:
    OCP_PVCO        PVCO;
    OCP_PVDG        PVDG;
    OCP_PVTW        PVTW;
};

#endif /* end if __MIXTUREBO_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/15/2021      Format file                          */
/*----------------------------------------------------------------------------*/
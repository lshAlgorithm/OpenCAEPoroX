/*! \file    OCPMixture.hpp
 *  \brief   OCPMixture class declaration
 *  \author  Shizhe Li
 *  \date    Jul/12/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __OCPMIXTURE_HEADER__
#define __OCPMIXTURE_HEADER__

#include "OCPConst.hpp"

#include <vector>

using namespace std;


enum class OCPMixtureType
{
    /// single phase
    SP,
    /// blackoil: oil, gas
    BO_OG,
    /// blackoil: oil, water
    BO_OW,
    /// blackoil: gas and water
    BO_GW,
    /// blackoil: oil, gas and water
    BO_OGW,
    /// Compositional Model with const temperature
    COMP,
    /// Compositional Model with variable temperature
    COMPT,
    /// Thermal-K: oil and water
    THERMALK_OW
};


enum class PhaseType
{
    /// oil phase
    oil,
    /// gas phase
    gas,
    /// water phase
    wat
};


/// mixture varset
class OCPMixtureVarSet
{
public:
    OCPMixtureVarSet() = default;
    void Init(const USI& numPhase, const USI& numCom, const OCPMixtureType& mixType) {

        switch (mixType)
        {
        case OCPMixtureType::BO_OG:
            o = 0;
            g = 1;
            w = -1;
            break;
        case OCPMixtureType::BO_OW:
        case OCPMixtureType::THERMALK_OW:
            o = 0;
            w = 1;
            g = -1;
            break;
        case OCPMixtureType::BO_GW:           
            g = 0;
            w = 1;
            o = -1;
            break;
        case OCPMixtureType::BO_OGW:
        case OCPMixtureType::COMP:
            o = 0;
            g = 1;
            w = 2;
            break;
        default:
            OCP_ABORT("Inavailable Mixture Type!");
            break;
        }
        if (o >= 0) l.push_back(o);
        if (w >= 0) l.push_back(w);

        OCP_BOOL ifThermal = OCP_FALSE;
        if (mixType == OCPMixtureType::THERMALK_OW ||
            mixType == OCPMixtureType::COMPT) {
            ifThermal = OCP_TRUE;
        }

        // vars
        np = numPhase;
        nc = numCom;
        Ni.resize(nc);
        phaseExist.resize(np);
        S.resize(np);
        nj.resize(np);
        vj.resize(np);
        x.resize(np * nc);
        rho.resize(np);
        xi.resize(np);
        mu.resize(np);

        // derivatives
        vfi.resize(nc);
        vjP.resize(np);
        if (ifThermal) vjT.resize(np);
        vji.resize(np);
        for (auto& v : vji) v.resize(nc);
        rhoP.resize(np);
        if (ifThermal) rhoT.resize(np);
        rhox.resize(np * nc);
        xiP.resize(np);
        if (ifThermal) xiT.resize(np);
        xix.resize(np * nc);
        muP.resize(np);
        if (ifThermal) muT.resize(np);
        mux.resize(np * nc);
        if (ifThermal) Ufi.resize(nc);
        if (ifThermal) H.resize(np);
        if (ifThermal) HT.resize(np);
        if (ifThermal) Hx.resize(np * nc);

        if (ifThermal) dXsdXp.resize(np * (nc + 1) * (nc + 2));
        else           dXsdXp.resize(np * (nc + 1) * (nc + 1));
    }

public:
    /// Calculate total fluid volume and phase saturation with vj
    void CalVfS() {
        Vf = 0;
        for (USI j = 0; j < np; j++) {
            if (phaseExist[j]) {
                Vf += vj[j];
            }
        }
        for (USI j = 0; j < np; j++) {
            S[j] = 0;
            if (phaseExist[j]) {
                S[j] = vj[j] / Vf;
            }
        }
    }

public:
    const OCP_DBL* GetXj(const USI& j) const { return &x[j * nc]; }

public:
    // Index of phases
    /// oil, gas, water
    INT                     o, g, w;
    /// liquid              
    vector<INT>             l;

    /// num of phase, components
    USI                     np, nc;
    /// pressure, temperature
    OCP_DBL                 P, T;
    /// Buble point pressure 
    OCP_DBL                 Pb;
    /// total moles of components
    OCP_DBL                 Nt;
    /// total volume of components
    OCP_DBL                 Vf;
    /// mole number of components
    vector<OCP_DBL>         Ni;
    /// existence of phase
    vector<OCP_BOOL>        phaseExist;
    /// saturation of phase
    vector<OCP_DBL>         S;
    /// mole number of phases
    vector<OCP_DBL>         nj;
    /// volume of phases
    vector<OCP_DBL>         vj;
    /// molar fraction of component i in phase j
    vector<OCP_DBL>         x;
    /// mass density of phases
    vector<OCP_DBL>         rho;
    /// molar density of phases
    vector<OCP_DBL>         xi;
    /// viscosity of phases
    vector<OCP_DBL>         mu;

    // Derivatives (full derivatives)
    /// dVf / dP
    OCP_DBL                 vfP;
    /// dVf / dT
    OCP_DBL                 vfT; 
    /// dVf / dNi
    vector<OCP_DBL>         vfi;
    /// dVj / dP            
    vector<OCP_DBL>         vjP;
    /// dVj / dT            
    vector<OCP_DBL>         vjT;
    /// dVj / dNi
    vector<vector<OCP_DBL>> vji;

    // Derivatives (partial derivatives)
    /// drho / dP
    vector<OCP_DBL> rhoP;
    /// drho / dT
    vector<OCP_DBL> rhoT; 
    /// drho / dx 
    vector<OCP_DBL> rhox;
    /// dxi / dP
    vector<OCP_DBL> xiP;  
    /// dxi / dT
    vector<OCP_DBL> xiT;  
    /// dxi / dx
    vector<OCP_DBL> xix;
    /// dmu / dP
    vector<OCP_DBL> muP;  
    /// dmu / dT
    vector<OCP_DBL> muT;  
    /// dmu / dx
    vector<OCP_DBL> mux;  

    /// Internal energy of fluid
    OCP_DBL Uf;
    /// dUf / dP
    OCP_DBL UfP;
    /// dUf / dT
    OCP_DBL UfT;
    /// dUf / dNi
    vector<OCP_DBL> Ufi; 
    /// Enthalpy
    vector<OCP_DBL> H;  
    /// d Hj / d T
    vector<OCP_DBL> HT;  
    ///< d Hj / d xij
    vector<OCP_DBL> Hx;  

    /// d(Sj, xij) / d(P,Ni,(T))
    vector<OCP_DBL> dXsdXp;
};


/////////////////////////////////////////////////////
// OCPMixture
/////////////////////////////////////////////////////

class OCPMixture
{
public:
    OCPMixture() = default;
    auto MixtureType() const { return mixtureType; }
    auto IfBlkModel() const { return (mixtureType >= OCPMixtureType::SP) && (mixtureType < OCPMixtureType::COMP); }
    const OCPMixtureVarSet& GetVarSet() const { return vs; }
    virtual void CalVStd(const OCP_DBL& P, const OCP_DBL& T, const OCP_DBL* Ni) = 0;
    virtual OCP_DBL CalVmStd(const OCP_DBL& P, const OCP_DBL& T, const OCP_DBL* z, const PhaseType& pt) = 0;

public:
    auto OilIndex() const { return vs.o; }
    auto GasIndex() const { return vs.g; }
    auto WatIndex() const { return vs.w; }
    auto LiquidIndex() const { return vs.l; }

protected:
    /// mixture type
    OCPMixtureType   mixtureType;
    /// mixture variables set
    OCPMixtureVarSet vs;
};


#endif /* end if __OCPMIXTURE_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Jul/12/2023      Create file                          */
/*----------------------------------------------------------------------------*/
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

const USI OCPMIXTURE_BO_OG   = 121;
const USI OCPMIXTURE_BO_OW   = 122;
const USI OCPMIXTURE_BO_GW   = 123;
const USI OCPMIXTURE_BO_OGW  = 13;
const USI OCPMIXTURE_COMP    = 2;
const USI OCPMIXTURE_THERMAL = 3;

/// mixture varset
class OCPMixtureVarSet
{
public:
    OCPMixtureVarSet(const USI& numPhase, const USI& numCom) { Init(numPhase, numCom); }
    void Init(const USI& numPhase, const USI& numCom) {
        // vars
        np = numPhase;
        nc = numCom;
        Ni.resize(nc);
        phaseExist.resize(np);
        S.resize(np);
        nj.resize(np);
        vj.resize(np);
        xij.resize(np * nc);
        rho.resize(np);
        xi.resize(np);
        mu.resize(np);

        // derivatives
        vfi.resize(nc);
        vjP.resize(np);
        vjT.resize(np);
        vji.resize(np * nc);
        rhoP.resize(np);
        rhoT.resize(np);
        rhox.resize(np * nc);
        xiP.resize(np);
        xiT.resize(np);
        xix.resize(np * nc);
        muP.resize(np);
        muT.resize(np);
        mux.resize(np * nc);
    }

public:
    /// num of phase, components
    USI np, nc;
    /// pressure, temperature
    OCP_DBL P, T;
    /// total moles of components
    OCP_DBL Nt;
    /// total volume of components
    OCP_DBL Vf;
    /// mole number of components
    vector<OCP_DBL> Ni;
    /// existence of phase
    vector<OCP_BOOL> phaseExist; 
    /// saturation of phase
    vector<OCP_DBL> S;
    /// mole number of phases
    vector<OCP_DBL> nj;
    /// volume of phases
    vector<OCP_DBL> vj;
    /// molar fraction of component i in phase j
    vector<OCP_DBL>  xij;   
    /// mass density of phases
    vector<OCP_DBL>  rho;    
    /// molar density of phases
    vector<OCP_DBL>  xi;   
    /// viscosity of phases
    vector<OCP_DBL>  mu;

    // Derivatives (full derivatives)
    /// dVf / dP
    OCP_DBL vfP;
    /// dVf / dT
    OCP_DBL vfT; 
    /// dVf / dNi
    vector<OCP_DBL> vfi;
    /// dVj / dP
    vector<OCP_DBL> vjP;
    /// dVj / dT
    vector<OCP_DBL> vjT;
    /// dVj / dNi
    vector<OCP_DBL> vji;

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

};


/////////////////////////////////////////////////////
// OCPMixture
/////////////////////////////////////////////////////

class OCPMixture
{
public:
    OCPMixture() = default;
    USI MixtureType() const { return mixtureType; }
    OCPMixtureVarSet& GetVarSet() { return vs; }

protected:
    USI              mixtureType;
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
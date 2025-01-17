/*! \file    HeatConduct.hpp
 *  \brief   HeatConduct class declaration
 *  \author  Shizhe Li
 *  \date    Aug/26/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __HEATCONDUCT_HEADER__
#define __HEATCONDUCT_HEADER__


 // OpenCAEPoroX header files
#include "ParamReservoir.hpp"
#include "BulkVarSet.hpp"
#include "BulkConnVarSet.hpp"

#include <vector>

using namespace std;


class HeatConductVarSet
{
    friend class HeatConduct;
    friend class HeatConductMethod01;

public:
    void SetUp(const OCP_USI& nbin, const USI& npin, const INT& oIndex, const INT& gIndex, const INT& wIndex) {
        nb = nbin; 
        np = npin;
        o  = oIndex;
        g  = gIndex;
        w  = wIndex;
    }
    void ResetToLastTimeStep()
    {
        kt  = lkt;
        ktP = lktP;
        ktT = lktT;
        ktS = lktS;
    }
    void UpdateLastTimeStep()
    {
        lkt  = kt;
        lktP = ktP;
        lktT = ktT;
        lktS = ktS;
    }

public:
    /// number of bulks
    OCP_USI         nb;
    /// number of phases
    USI             np;
    /// oil, gas, water index
    INT             o, g, w;
    /// thermal conduction term
    OCP_DBL         conduct_H;
    /// thermal conductivity
    vector<OCP_DBL> kt;
    /// dkt / dP
    vector<OCP_DBL> ktP;
    /// dkt / dT
    vector<OCP_DBL> ktT;
    /// dkt / dS
    vector<OCP_DBL> ktS;
    /// thermal conductivity
    vector<OCP_DBL> lkt;
    /// dkt / dP
    vector<OCP_DBL> lktP;
    /// dkt / dT
    vector<OCP_DBL> lktT;
    /// dkt / dS
    vector<OCP_DBL> lktS;
};


class HeatConductMethod
{
public:
    HeatConductMethod() = default;
    virtual void CalConductCoeff(const OCP_USI& bId, HeatConductVarSet& hcvs, const BulkVarSet& bvs) const = 0;
    virtual OCP_DBL CalFlux(const HeatConductVarSet& hcvs, const BulkConnPair& bp, const BulkVarSet& bvs) const = 0;
    virtual void AssembleMatFIM(const BulkConnPair& bp, const HeatConductVarSet& hcvs, const BulkVarSet& bvs, FluxVarSet& fvs) const = 0;
};


class HeatConductMethod01 : public HeatConductMethod
{
public:
    HeatConductMethod01(const ParamReservoir& rs_param, HeatConductVarSet& hcvs);
    void CalConductCoeff(const OCP_USI& bId, HeatConductVarSet& hcvs, const BulkVarSet& bvs) const override;
    OCP_DBL CalFlux(const HeatConductVarSet& hcvs, const BulkConnPair& bp, const BulkVarSet& bvs) const override;
    void AssembleMatFIM(const BulkConnPair& bp, const HeatConductVarSet& hcvs, const BulkVarSet& bvs, FluxVarSet& fvs) const override;

protected:
    /// phase thermal conductivity
    vector<OCP_DBL> thconP;
    /// rock thermal conductivity
    OCP_DBL         thconR;
};


class HeatConduct
{
public:
    HeatConduct() = default;
    void Setup(const ParamReservoir& rs_param, const BulkVarSet& bvs);
    void CalConductCoeff(const BulkVarSet& bvs);
    void CalFlux(const BulkConnPair& bp, const BulkVarSet& bvs);
    void AssembleMatFIM(const BulkConnPair& bp, const BulkVarSet& bvs, FluxVarSet& fvs) const;
    const auto& GetVarSet() const { return vs; }
    auto& GetConductH() const { return vs.conduct_H; }
    void ResetToLastTimeStep() { if (ifUse)  vs.ResetToLastTimeStep(); }
    void UpdateLastTimeStep() { if (ifUse)  vs.UpdateLastTimeStep(); }


protected:
    /// If use heat conduction
    OCP_BOOL                   ifUse{ OCP_FALSE };
    /// Heat conduction varsets
    HeatConductVarSet          vs;
    /// method for heat conduction calculation
    vector<HeatConductMethod*> hcM;
};


#endif /* end if __HeatConduct_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Aug/26/2023      Create file                          */
/*----------------------------------------------------------------------------*/

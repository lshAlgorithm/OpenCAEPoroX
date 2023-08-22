/*! \file    PVTModule.hpp
 *  \brief   PVTModule class declaration
 *  \author  Shizhe Li
 *  \date    Aug/19/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __PVTMODULE_HEADER__
#define __PVTMODULE_HEADER__

 // Standard header files
#include <cassert>

// OpenCAEPoroX header files
#include "MixtureUnit.hpp"
#include "MixtureUnitBlkOil.hpp"
#include "MixtureUnitComp.hpp"
#include "MixtureUnitThermal.hpp"
#include "BulkVarSet.hpp"

using namespace std;


class PVTModule
{

public:
    void Setup(const ParamReservoir& rs_param, BulkVarSet& bvs, OptionalFeatures& opts);
    auto GetPVT(const OCP_USI& n) const { return PVTs[PVTNUM[n]]; }
    auto GetMixture() const {return PVTs[0]->GetMixture();}
    auto& GetPVTNUM() { return PVTNUM; }
    auto GetMixtureType() const { return mixType; }

protected:
    OCPMixtureType       mixType;
    /// number of PVT regions
    USI                  NTPVT;
    /// Index of PVT region for each bulk
    vector<USI>          PVTNUM;
    /// PVT modules
    vector<MixtureUnit*> PVTs;

};



#endif /* end if __PVTMODULE_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Aug/19/2023      Create file                          */
/*----------------------------------------------------------------------------*/
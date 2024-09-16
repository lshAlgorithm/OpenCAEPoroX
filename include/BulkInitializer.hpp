/*! \file    BulkInitializer.hpp
 *  \brief   BulkInitializer class declaration
 *  \author  Shizhe Li
 *  \date    Aug/26/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __BULKINITIALIZER_HEADER__
#define __BULKINITIALIZER_HEADER__


// OpenCAEPoroX header files
#include "BulkVarSet.hpp"
#include "PVTModule.hpp"
#include "SATModule.hpp"
#include "Domain.hpp"

#include <vector>

using namespace std;

/// Initial reservoir infomation for calculating initial equilibration.
//  Note: This class includes reference depth and pressure at it, depth of contacts
//  between phases, and capillary pressure at phase contact surfaces.
class Equil
{
    friend class BulkInitializer;
    friend class Bulk;
protected:
    /// The reference depth
    OCP_DBL  Dref; 
    /// Pressure at the reference depth
    OCP_DBL  Pref;
    /// The depth of oil-water contact surface
    OCP_DBL  DOWC;
    /// Capillary pressure at oil-water contact Pcow = Po - Pw
    OCP_DBL  PcOW;
    /// The depth of gas-oil contact surface
    OCP_DBL  DGOC; 
    /// capillary pressure at gas-oil contact Pcgo = Pg - Po
    OCP_DBL  PcGO;
    /// PBVD Table: bubble point pressure vs depth
    OCPTable PBVD;
};

/// Initialize the bulks
class BulkInitializer
{
public:
	void Setup(const ParamReservoir& rs_param, const OCPMixtureType& mixType);
    void Initialize(BulkVarSet& bvs, const PVTModule& PVTm, const SATModule& SATm, const OptionalModules& optMs, const Domain& domain);
    auto& GetSwat() { return swat; }

protected:
    /// initialize reservoir with hydrostatic equilibrium only
    void InitHydroEquil(BulkVarSet& bvs, const PVTModule& PVTm, const SATModule& SATm, const Domain& domain);
    /// initialize reservoir with water and hydrostatic equilibrium
    void InitHydroEquilW(BulkVarSet& bvs, const PVTModule& PVTm, const SATModule& SATm, const OptionalModules& optMs, const Domain& domain);

protected:
    /// Equilibration data specification
    vector<Equil>    EQUIL;
    /// Initial mole ratio of components vs. depth, table set
    vector<OCPTable> initZi_Tab; 
    /// Initial temperature vs. depth, table set
    vector<OCPTable> initT_Tab;
    /// number of nodes of P vs. Z table
    USI              numNodes{ 50 };

    /// initial reservoir water saturation
    vector<OCP_DBL>  swat;
};



#endif /* end if __BULKINITIALIZER_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Aug/26/2023      Create file                          */
/*----------------------------------------------------------------------------*/

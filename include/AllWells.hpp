/*! \file    AllWells.hpp
 *  \brief   AllWells class declaration
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __WELLGROUP_HEADER__
#define __WELLGROUP_HEADER__

// OpenCAEPoroX header files
#include "ParamReservoir.hpp"
#include "Well.hpp"
#include "WellPeaceman.hpp"

using namespace std;

/// WellGroup contains a well group, which is responsible for managing the production
/// and injection targets and interactions of some wells, etc. it will be initialized
/// in the beginning of simulation, if necessary, it should be updated, for example,
/// the types of well changes or the wells are regrouped.
class WellGroup
{
    friend class AllWells;

public:
    WellGroup() = default;
    WellGroup(const string& gname)
        : name(gname){};

private:
    string      name;    ///< name of wellGroup
    vector<USI> wId;     ///< Well index in wellGroup
    vector<USI> wIdINJ;  ///< Inj well index in AllWells
    vector<USI> wIdPROD; ///< Prod well index in AllWells

    /*
    OCP_DBL GOPR{0}; ///< Group oil production rate
    OCP_DBL GGPR{0}; ///< Group gas production rate
    OCP_DBL GWPR{0}; ///< Group water production rate
    OCP_DBL GOPT{0}; ///< Group oil total production
    OCP_DBL GGPT{0}; ///< Group gas total production
    OCP_DBL GWPT{0}; ///< Group water total production
    OCP_DBL GGIR{0}; ///< Group gas injection rate
    OCP_DBL GWIR{0}; ///< Group water injection rate
    OCP_DBL GGIT{0}; ///< Group gas total injection
    OCP_DBL GWIT{0}; ///< Group gas total injection
    */
};

/// AllWells contains all wells now, it's used to manages all wells uniformly in
/// reservoirs. actually, you can regard it as an interface between wells and other
/// modules.
class AllWells
{
    // temp
    friend class Reservoir;
    friend class OCPNRsuite;
    friend class Out4RPT;
    friend class Out4VTK;

    friend class IsoT_FIM;
    friend class IsoT_IMPEC;
    friend class IsoT_AIMc;
    friend class T_FIM;

public:
    AllWells() = default;

    /////////////////////////////////////////////////////////////////////
    // Input Param and Setup
    /////////////////////////////////////////////////////////////////////

public:
    /// Input well param
    void InputParam(const ParamWell& paramWell, const Domain& domain);
    /// Setup wells
    void Setup(const Bulk& bk);
    /// Setup information of wellGroup
    void SetupWellGroup(const Bulk& bk);
    /// Setup bulks which are penetrated by wells at current stage
    void SetupWellBulk(Bulk& bk) const;

    /////////////////////////////////////////////////////////////////////
    // General
    /////////////////////////////////////////////////////////////////////

public:
    /// Apply the operation mode at the ith critical time.
    void ApplyControl(const USI& i);
    /// Set the initial well pressure
    void InitBHP(const Bulk& bk);
    /// Calculate well properties at the beginning of each time step.
    void PrepareWell(const Bulk& bk);
    /// Calculate volume flow rate and moles flow rate of each perforation.
    void CalFlux(const Bulk& bk);
    /// Calculate Injection rate, total Injection, Production rate, total Production
    void CalIPRT(const Bulk& bk, OCP_DBL dt);
    void UpdateLastTimeStep() {
        for (auto& w : wells) w->UpdateLastTimeStep();
    }
    void ResetToLastTimeStep(const Bulk& bk) {
        for (auto& w : wells) w->ResetToLastTimeStep(bk);
    }
    /// Check if unreasonable well pressure or perforation pressure occurs.
    ReservoirState CheckP(const Bulk& bk);
    /// Return the num of wells.
    USI GetWellNum() const { return numWell; }
    /// Return the name of specified well.
    string GetWellName(const USI& i) const { return wells[i]->name; }
    /// Return the index of specified well.
    USI GetIndex(const string& name) const;
    /// Return the num of perforations of well i.
    USI GetWellPerfNum(const USI& i) const { return wells[i]->numPerf; }
    /// Return the num of perforations of all wells
    USI GetWellPerfNum() const;
    /// Calculate maximum num of perforations of all Wells.
    USI     GetMaxWellPerNum() const;

    /// Return the BHP of wth well.
    OCP_DBL GetWBHP(const USI& w) const
    {
        if (wells[w]->IsOpen())  return wells[w]->bhp;
        else                     return 0;
    }
    void    ShowWellStatus(const Bulk& bk)
    {
        for (USI w = 0; w < numWell; w++) wells[w]->ShowPerfStatus(bk);
    }
    OCP_BOOL    GetWellOptChange() const { return wellOptChange; }
    USI GetNumOpenWell()const;

protected:
    USI                     numWell;   ///< num of wells.
    vector<Well*>           wells;     ///< well set.
    USI                     numGroup;  ///< num of groups
    vector<WellGroup>       wellGroup; ///< wellGroup set

    OCP_BOOL           wellOptChange; ///< if wells change, then OCP_TRUE
    vector<SolventINJ> solvents;   ///< Sets of Solvent
    OCP_DBL            dPmax{0};   ///< Maximum BHP change

    OCP_DBL          Psurf;    ///< well reference pressure
    OCP_DBL          Tsurf;    ///< well reference temperature

    /////////////////////////////////////////////////////////////////////
    // Injection/Production Rate
    /////////////////////////////////////////////////////////////////////

public:
    /// Return gas water injection in field.
    OCP_DBL GetFGIT() const { return FGIT; }
    /// Return water injection rate in field.
    OCP_DBL GetFWIR() const { return FWIR; }
    /// Return total water injection in field.
    OCP_DBL GetFWIT() const { return FWIT; }
    /// Return oil production rate in field.
    OCP_DBL GetFOPR() const { return FOPR; }
    /// Return total oil production in field.
    OCP_DBL GetFOPT() const { return FOPT; }
    /// Return gas production rate in field.
    OCP_DBL GetFGPR() const { return FGPR; }
    /// Return total gas production in field.
    OCP_DBL GetFGPT() const { return FGPt; }
    /// Return water production rate in field.
    OCP_DBL GetFWPR() const { return FWPR; }
    /// Return total water production in field.
    OCP_DBL GetFWPT() const { return FWPT; }
    /// Return gas injection rate in field.
    OCP_DBL GetFGIR() const { return FGIR; }

    /// Return gas injection rate of the wth well.
    OCP_DBL GetWGIR(const USI& w) const { return wells[w]->WGIR; }
    /// Return total gas injection of the wth well.
    OCP_DBL GetWGIT(const USI& w) const { return wells[w]->WGIT; }
    /// Return water injection rate of the wth well.
    OCP_DBL GetWWIR(const USI& w) const { return wells[w]->WWIR; }
    /// Return total water injection of the wth well.
    OCP_DBL GetWWIT(const USI& w) const { return wells[w]->WWIT; }
    /// Return oil production rate of the wth well.
    OCP_DBL GetWOPR(const USI& w) const { return wells[w]->WOPR; }
    /// Return total oil production of the wth well.
    OCP_DBL GetWOPT(const USI& w) const { return wells[w]->WOPT; }
    /// Return gas production rate of the wth well.
    OCP_DBL GetWGPR(const USI& w) const { return wells[w]->WGPR; }
    /// Return total gas production of the wth well.
    OCP_DBL GetWGPT(const USI& w) const { return wells[w]->WGPT; }
    /// Return water production rate of the wth well.
    OCP_DBL GetWWPR(const USI& w) const { return wells[w]->WWPR; }
    /// Return total water production of the wth well.
    OCP_DBL GetWWPT(const USI& w) const { return wells[w]->WWPT; }

protected:
    OCP_DBL FGIR{0}; ///< gas injection rate in field.
    OCP_DBL FGIT{0}; ///< gas total injection in field.
    OCP_DBL FWIR{0}; ///< water injection rate in field.
    OCP_DBL FWIT{0}; ///< water total injection in field.
    OCP_DBL FOPR{0}; ///< oil production rate in field.
    OCP_DBL FOPT{0}; ///< oil total production in field.
    OCP_DBL FGPR{0}; ///< gas production rate in field.
    OCP_DBL FGPt{0}; ///< gas total production in field.
    OCP_DBL FWPR{0}; ///< water production rate in field.
    OCP_DBL FWPT{0}; ///< water total production in field.

    // for output
private:
    OCP_BOOL              useVTK{OCP_FALSE};
    // vector<OCPpolyhedron> polyhedronWell;
    // When the well is under the BHP control, then give it's BHP
    // When the well is under the RATE control, then give it's RATE
    mutable vector<OCP_DBL> wellVal; ///< characteristics for well

public:
    // void SetPolyhedronWell(const Grid& myGrid);
    void SetWellVal() const;
};

#endif /* end if __WELLGROUP_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/15/2021      Format file                          */
/*  Shizhe Li           Feb/08/2022      Rename to AllWells                   */
/*----------------------------------------------------------------------------*/
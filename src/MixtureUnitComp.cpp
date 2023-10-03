/*! \file    MixtureUnitComp.cpp
 *  \brief   MixtureUnitComp class definition for compositional models
 *  \author  Shizhe Li
 *  \date    Jan/05/2022
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "MixtureUnitComp.hpp"


MixtureUnitComp::MixtureUnitComp(const ParamReservoir& rs_param, const USI& i, OptionalModules& opts)
{
    compM = new OCPMixtureComp(rs_param, i, opts);
    vs    = &compM->GetVarSet();

    /// Optional Features
    // Calculate surface tension
    surTen          = &opts.surTen;
    stMethodIndex   = surTen->Setup(rs_param, i, opts.nb);
    // Miscible Factor
    misFac          = &opts.misFac;
    mfMethodIndex   = misFac->Setup(rs_param, i, opts.nb, surTen);
}

void MixtureUnitComp::Flash(const OCP_DBL& Pin, const OCP_DBL& Tin, const OCP_DBL* Niin)
{
    compM->Flash(Pin, Tin, Niin);
}


void MixtureUnitComp::InitFlashIMPEC(const OCP_USI& bId, const BulkVarSet& bvs)
{
    compM->InitFlash(bId, bvs);
    surTen->CalSurfaceTension(bId, stMethodIndex, *vs);
    misFac->CalMiscibleFactor(bId, mfMethodIndex);
}

void MixtureUnitComp::InitFlashFIM(const OCP_USI& bId, const BulkVarSet& bvs)
{
    compM->InitFlashDer(bId, bvs);
    surTen->CalSurfaceTension(bId, stMethodIndex, *vs);
    misFac->CalMiscibleFactor(bId, mfMethodIndex);
}

void MixtureUnitComp::FlashIMPEC(const OCP_USI& bId, const BulkVarSet& bvs)
{
    compM->Flash(bId, bvs);
    surTen->CalSurfaceTension(bId, stMethodIndex, *vs);
    misFac->CalMiscibleFactor(bId, mfMethodIndex);
}

void MixtureUnitComp::FlashFIM(const OCP_USI& bId, const BulkVarSet& bvs)
{
    compM->FlashDer(bId, bvs);
    surTen->CalSurfaceTension(bId, stMethodIndex, *vs);
    misFac->CalMiscibleFactor(bId, mfMethodIndex);
}


OCP_DBL MixtureUnitComp::XiPhase(const OCP_DBL& Pin,
                         const OCP_DBL& Tin,
                         const vector<OCP_DBL>& Ziin,
                         const PhaseType& pt)
{
    return compM->CalXi(Pin, 0, Tin, &Ziin[0], pt);
}

OCP_DBL MixtureUnitComp::RhoPhase(const OCP_DBL& Pin,
                      const OCP_DBL& Pbb,
                      const OCP_DBL& Tin,
                      const vector<OCP_DBL>& Ziin,
                      const PhaseType& pt)
{
    return compM->CalRho(Pin, Pbb, Tin, &Ziin[0], pt);
}



/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Jan/05/2022      Create file                          */
/*----------------------------------------------------------------------------*/
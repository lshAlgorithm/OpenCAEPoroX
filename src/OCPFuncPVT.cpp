/*! \file    OCPFuncPVT.cpp
 *  \brief   Functions for PVT in OCP
 *  \author  Shizhe Li
 *  \date    Jun/18/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */


 // OpenCAEPoroX header files
#include "OCPFuncPVT.hpp"


/////////////////////////////////////////////////////
// PVTW
/////////////////////////////////////////////////////


OCP_DBL OCP_PVTW::CalBw(const OCP_DBL& P)
{
	table.GetCloseRow(0, P, data);
	const OCP_DBL Pref  = data[0];
	const OCP_DBL bref  = data[1];
	const OCP_DBL Cbref = data[2];
	return bref * (1 - Cbref * (P - Pref));
}


void OCP_PVTW::CalRhoXiMuDer(const OCP_DBL& P, OCP_DBL& rho, OCP_DBL& xi, OCP_DBL& mu,
	OCP_DBL& rhoP, OCP_DBL& xiP, OCP_DBL& muP)
{
	OCP_DBL b, bp;
	CalBwMuwDer(P, b, mu, bp, muP);
	xi   = 1 / CONV1 / b;
	rho  = stdRhoW / b;

	xiP  = -1 / CONV1 * bp / (b * b);
	rhoP = -stdRhoW * bp / (b * b);
}


void OCP_PVTW::CalBwMuwDer(const OCP_DBL& P, OCP_DBL& b, OCP_DBL& mu, OCP_DBL& bP, OCP_DBL& muP)
{
	table.GetCloseRow(0, P, data);
	const OCP_DBL Pref   = data[0];
	const OCP_DBL bref   = data[1];
	const OCP_DBL Cbref  = data[2];
	const OCP_DBL muref  = data[3];
	const OCP_DBL Cmuref = data[4];
	b   = bref * (1 - Cbref * (P - Pref));
	bP  = -Cbref * bref;
	mu  = muref * (1 + Cmuref * (P - Pref));
	muP = Cmuref * muref;
}

/////////////////////////////////////////////////////
// PVCO
/////////////////////////////////////////////////////


OCP_DBL OCP_PVCO::CalRhoo(const OCP_DBL& P, const OCP_DBL& Pb)
{
	table.Eval_All(0, Pb, data);
	const OCP_DBL rssat = data[1];
	const OCP_DBL bosat = data[2];
	const OCP_DBL cbosat = data[4];
	const OCP_DBL bo = bosat * (1 - cbosat * (P - Pb));
	const OCP_DBL rhoO   = (stdRhoO + (1000 / CONV1) * rssat * stdRhoG) / bo;
	return rhoO;
}


/////////////////////////////////////////////////////
// PVDG
/////////////////////////////////////////////////////


void OCP_PVDG::CalRhoXiMuDer(const OCP_DBL& P, OCP_DBL& rho, OCP_DBL& xi, OCP_DBL& mu,
	OCP_DBL& rhoP, OCP_DBL& xiP, OCP_DBL& muP)
{
	OCP_DBL b, bp;
	CalBgMugDer(P, b, mu, bp, muP);
	xi   = 1 / CONV1 / b;
	rho  = 1000 / CONV1 * stdRhoG / b;
	
	xiP  = -1 / CONV1 * bp / (b * b);
	rhoP = -1000 / CONV1 * stdRhoG * bp / (b * b);
}

OCP_DBL OCP_PVDG::CalBg(const OCP_DBL& P)
{
	return table.Eval(0, P, 1);
}

void OCP_PVDG::CalBgMugDer(const OCP_DBL& P, OCP_DBL& b, OCP_DBL& mu, OCP_DBL& bP, OCP_DBL& muP)
{
	table.Eval_All(0, P, data, cdata);
	b   = data[1];
	mu  = data[2];
	bP  = cdata[1];
	muP = cdata[2];
}


/////////////////////////////////////////////////////
// PVDO
/////////////////////////////////////////////////////


OCP_DBL OCP_PVDO::CalBo(const OCP_DBL& P)
{
	return table.Eval(0, P, 1);
}


void OCP_PVDO::CalRhoXiMuDer(const OCP_DBL& P, OCP_DBL& rho, OCP_DBL& xi, OCP_DBL& mu,
	OCP_DBL& rhoP, OCP_DBL& xiP, OCP_DBL& muP)
{
	OCP_DBL b, bp;
	CalBoMuoDer(P, b, mu, bp, muP);
	xi   = 1 / CONV1 / b;
	rho  = stdRhoO / b;

	xiP  = - 1 / CONV1 * bp / (b * b);
	rhoP = -stdRhoO * bp / (b * b);
}


void OCP_PVDO::CalBoMuoDer(const OCP_DBL& P, OCP_DBL& bo, OCP_DBL& muo, OCP_DBL& dBodP, OCP_DBL& dMuodP)
{
	table.Eval_All(0, P, data, cdata);
	bo     = data[1];
	muo    = data[2];
	dBodP  = cdata[1];
	dMuodP = cdata[2];
}


/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Jun/18/2023      Create file                          */
/*----------------------------------------------------------------------------*/


/*! \file    OCPSATFunc.hpp
 *  \brief   Functions for Saturations in OCP
 *  \author  Shizhe Li
 *  \date    Jun/29/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __OCPSATFUNC_HEADER__
#define __OCPSATFUNC_HEADER__

 // OpenCAEPoroX header files
#include "OCPTable.hpp"

using namespace std;


/////////////////////////////////////////////////////
// SWOF
/////////////////////////////////////////////////////



class OCP_SWOF
{
	/// 0th column: The water saturation (Sw)
	/// 1th column: The corresponding water relative permeability (Krw)
	/// 2th column: The corresponding oil relative permeability when only oil and water are present (Krow)
	/// 3th column: The corresponding water-oil capillary pressure (Pcow = Po - Pw) (bars (METRIC), psi (FIELD))
	///             Values should be level or decreasing down the column.

public:
	/// default constructor
	OCP_SWOF() = default;
	/// Setup internal table with 2D table
	void     Setup(const vector<vector<OCP_DBL>>& src);
	/// If table is empty
	OCP_BOOL IsEmpty() const { return table.IsEmpty(); }
	/// Return the connate saturation
	OCP_DBL  GetSwco() const { return table.GetCol(0)[0]; }
	/// Return the critical saturation
	OCP_DBL  GetSwcr() const;
	/// Return maximum capillary pressure Pcow: Po - Pw
	OCP_DBL  GetMaxPc() const { return table.GetCol(3).front(); }
	/// Return minimum capillary pressure Pcow: Po - Pw
	OCP_DBL  GetMinPc() const { return table.GetCol(3).back(); }
	/// Return the oil relative permeability in the presence of connate water only
	OCP_DBL  GetKrocw() const { return table.GetCol(2)[0]; }
	/// Return Sw
	const vector<OCP_DBL>& GetSw() const { return table.GetCol(0); }
	/// Return Pcow
	const vector<OCP_DBL>& GetPcow() const { return table.GetCol(3); }
	/// Return corresponding Sw with Pcow
	OCP_DBL  CalSw(const OCP_DBL& Pcow) const { return table.Eval_Inv(3, Pcow, 0); }
	/// Return corresponding Pcow with Sw
	OCP_DBL  CalPcow(const OCP_DBL& Sw) const { return table.Eval(0, Sw, 3); }

	/// Return corresponding Krw, Krow, Pcwo with Sw
	void     CalKrwKrowPcwo(const OCP_DBL& Sw, OCP_DBL& krw, OCP_DBL& krow, OCP_DBL& Pcwo) {
		table.Eval_All(0, Sw, data);
		krw  =  data[1];
		krow =  data[2];
		Pcwo = -data[3];
	}
	/// Return corresponding Krw, Krow, Pcwo and derivatives with Sw 
	void     CalKrwKrowPcwoDer(const OCP_DBL& Sw, 
		OCP_DBL& krw, OCP_DBL& krow, OCP_DBL& Pcwo, 
		OCP_DBL& dkrwdSw, OCP_DBL& dkrowdSw, OCP_DBL& dPcwodSw) {
		table.Eval_All(0, Sw, data, cdata);
		krw       =  data[1];
		krow      =  data[2];
		Pcwo      = -data[3];
		dkrwdSw   =  cdata[1];
		dkrowdSw  =  cdata[2];
		dPcwodSw  = -cdata[3];
	}

protected:
	OCPTable         table; ///< 2D table of SWOF
	vector<OCP_DBL>  data;  ///< container to store the values of interpolation.
	vector<OCP_DBL>  cdata; ///< container to store the slopes of interpolation.
};


/////////////////////////////////////////////////////
// SGOF
/////////////////////////////////////////////////////

class OCP_SGOF
{
	/// 0th column: The gas saturation (Sg)
	/// 1th column: The corresponding gas relative permeability (Krg)
	/// 2th column: The corresponding oil relative permeability when oil, gas and connate water are present. (Krog)
	/// 3th column: The correspondin oil-gas capillary pressure (Pcgo = Pg - Po) (bars (METRIC), psi (FIELD))
	///             Values should be level or increasing down the column.
public:
	/// default constructor
	OCP_SGOF() = default;
	/// Setup internal table with 2D table
	void     Setup(const vector<vector<OCP_DBL>>& src);
	/// If table is empty
	OCP_BOOL IsEmpty() const { return table.IsEmpty(); }
	/// Return the critical saturation
	OCP_DBL  GetSgcr() const;
	/// Return corresponding Sg with Pcgo
	OCP_DBL  CalSg(const OCP_DBL& Pcgo) const { return table.Eval(3, Pcgo, 0); }
	/// Return corresponding Krg with Sg
	OCP_DBL  CalKrg(const OCP_DBL& Sg) const { return table.Eval(0, Sg, 1); }
	/// Return corresponding Krg and derivatives with Sg
	OCP_DBL  CalKrg(const OCP_DBL& Sg, OCP_DBL& dKrgdSg) const { return table.Eval(0, Sg, 1, dKrgdSg); }
	/// Return corresponding Pcgo with Sg
	OCP_DBL  CalPcgo(const OCP_DBL& Sg) const { return table.Eval(0, Sg, 3); }

	/// Return corresponding Krw, Krow, Pcwo with Sw
	void     CalKrgKrogPcgo(const OCP_DBL& Sg, OCP_DBL& krg, OCP_DBL& krog, OCP_DBL& Pcgo) {
		table.Eval_All(0, Sg, data);
		krg  = data[1];
		krog = data[2];
		Pcgo = data[3];
	}
	/// Return corresponding Krw, Krow, Pcwo and derivatives with Sw 
	void     CalKrgKrogPcgoDer(const OCP_DBL& Sg,
		OCP_DBL& krg, OCP_DBL& krog, OCP_DBL& Pcgo,
		OCP_DBL& dkrgdSg, OCP_DBL& dkrogdSg, OCP_DBL& dPcgodSg) {
		table.Eval_All(0, Sg, data, cdata);
		krg      = data[1];
		krog     = data[2];
		Pcgo     = data[3];
		dkrgdSg  = cdata[1];
		dkrogdSg = cdata[2];
		dPcgodSg = cdata[3];
	}

protected:
	OCPTable         table; ///< 2D table of SWOF
	vector<OCP_DBL>  data;  ///< container to store the values of interpolation.
	vector<OCP_DBL>  cdata; ///< container to store the slopes of interpolation.
};


/////////////////////////////////////////////////////
// SOF3
/////////////////////////////////////////////////////

class OCP_SOF3
{
	/// 0th column: The oil saturation (So)
	/// 1th column: The corresponding oil relative permeability for regions where only oil and water are present (krow)
	/// 3th column: The corresponding oil relative permeability for regions where only oil, gas and connate water are
	///             present (krog)
public:
	OCP_SOF3() = default;
	/// Setup internal table with 2D table
	void     Setup(const vector<vector<OCP_DBL>>& src);
	/// If table is empty
	OCP_BOOL IsEmpty() const { return table.IsEmpty(); }
	/// Return the oil relative permeability in the presence of connate water only
	OCP_DBL  GetKrocw() const { return table.GetCol(1).back(); }

	/// Return corresponding Krow, krog with So
	void     CalKrowKrog(const OCP_DBL& So, OCP_DBL& krow, OCP_DBL& krog) {
		table.Eval_All(0, So, data);
		krow = data[1];
		krog = data[2];
	}
	/// Return corresponding Krw, Krow, Pcwo and derivatives with So 
	void     CalKrowKrogDer(const OCP_DBL& So, OCP_DBL& krow, OCP_DBL& krog, 
		OCP_DBL& dKrowdSo, OCP_DBL& dKrogdSo) {
		table.Eval_All(0, So, data, cdata);
		krow     = data[1];
		krog     = data[2];
		dKrowdSo = cdata[1];
		dKrogdSo = cdata[2];
	}

protected:
	OCPTable         table; ///< 2D table of SOF3
	vector<OCP_DBL>  data;  ///< container to store the values of interpolation.
	vector<OCP_DBL>  cdata; ///< container to store the slopes of interpolation.
};


/////////////////////////////////////////////////////
// SGFN
/////////////////////////////////////////////////////

class OCP_SGFN
{
	/// 0th column: The gas saturation (Sg)
	/// 1th column: The corresponding gas relative permeability (krg)
	/// 3th column: The corresponding oil-gas capillary pressure (Pcgo = Pg - Po)
public:
	OCP_SGFN() = default;
	/// Setup internal table with 2D table
	void     Setup(const vector<vector<OCP_DBL>>& src);
	/// If table is empty
	OCP_BOOL IsEmpty() const { return table.IsEmpty(); }
	/// Return the oil relative permeability in the presence of connate water only
	OCP_DBL  GetKrocw() const { return table.GetCol(1).back(); }
	/// Return corresponding Sg with Pcgo
	OCP_DBL  CalSg(const OCP_DBL& Pcgo) const { return table.Eval(2, Pcgo, 0); }
	/// Return corresponding Pcgo with Sg
	OCP_DBL  CalPcgo(const OCP_DBL& Sg) const { return table.Eval(0, Sg, 2); }
	/// Return corresponding Krg and derivatives with Sg
	OCP_DBL  CalKrg(const OCP_DBL& Sg, OCP_DBL& dKrgdSg) const {return table.Eval(0, Sg, 1, dKrgdSg);}

	/// Return corresponding Krg, Pcgo with Sg
	void     CalKrgPcgo(const OCP_DBL& Sg, OCP_DBL& krg, OCP_DBL& Pcgo) {
		table.Eval_All(0, Sg, data);
		krg  = data[1];
		Pcgo = data[2];
	}
	/// /// Return corresponding Krg, Pcgo and derivatives with Sg 
	void     CalKrgPcgoDer(const OCP_DBL& Sg, OCP_DBL& krg, OCP_DBL& Pcgo,
		OCP_DBL& dKrgdSg, OCP_DBL& dPcgodSg) {
		table.Eval_All(0, Sg, data, cdata);
		krg      = data[1];
		Pcgo     = data[2];
		dKrgdSg  = cdata[1];
		dPcgodSg = cdata[2];
	}

protected:
	OCPTable         table; ///< 2D table of SOF3
	vector<OCP_DBL>  data;  ///< container to store the values of interpolation.
	vector<OCP_DBL>  cdata; ///< container to store the slopes of interpolation.
};


/////////////////////////////////////////////////////
// SWFN
/////////////////////////////////////////////////////

class OCP_SWFN
{
	/// 0th column: The water saturation (Sw)
	/// 1th column: The corresponding water relative permeability (krw)
	/// 3th column: The corresponding water-oil capillary pressure (Pcow = Po - Pw)
public:
	OCP_SWFN() = default;
	/// Setup internal table with 2D table
	void     Setup(const vector<vector<OCP_DBL>>& src);
	/// If table is empty
	OCP_BOOL IsEmpty() const { return table.IsEmpty(); }
	/// Return the connate saturation
	OCP_DBL  GetSwco() const { return table.GetCol(0)[0]; }
	/// Return maximum pcow
	OCP_DBL  GetMaxPc() const { return table.GetCol(2)[0]; }
	/// Return minimum pcow
	OCP_DBL  GetMinPc() const { return table.GetCol(2).back(); }
	/// Return corresponding Sw with Pcow
	OCP_DBL  CalSw(const OCP_DBL& Pcow) const { return table.Eval_Inv(2, Pcow, 0); }
	/// Return corresponding Pcow with Sw
	OCP_DBL  CalPcow(const OCP_DBL& Sw) const { return table.Eval(0, Sw, 2); }
	/// Return Sw
	const vector<OCP_DBL>& GetSw() const { return table.GetCol(0); }
	/// Return Pcow
	const vector<OCP_DBL>& GetPcow() const { return table.GetCol(2); }

	/// Return corresponding Krw, Pcwo with Sw
	void     CalKrwPcwo(const OCP_DBL& Sw, OCP_DBL& krw, OCP_DBL& Pcwo) {
		table.Eval_All(0, Sw, data);
		krw  = data[1];
		Pcwo = -data[2];
	}
	/// /// Return corresponding Krw, Pcwo and derivatives with Sw 
	void     CalKrwPcwoDer(const OCP_DBL& Sw, OCP_DBL& krw, OCP_DBL& Pcwo,
		OCP_DBL& dKrwdSw, OCP_DBL& dPcwodSw) {
		table.Eval_All(0, Sw, data, cdata);
		krw      = data[1];
		Pcwo     = -data[2];
		dKrwdSw  = cdata[1];
		dPcwodSw = -cdata[2];
	}

protected:
	OCPTable         table; ///< 2D table of SOF3
	vector<OCP_DBL>  data;  ///< container to store the values of interpolation.
	vector<OCP_DBL>  cdata; ///< container to store the slopes of interpolation.
};




#endif // __OCPSATFUNC_HEADER__

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Jun/29/2023      Create file                          */
/*----------------------------------------------------------------------------*/


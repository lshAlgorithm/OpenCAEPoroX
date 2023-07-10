/*! \file    FlowUnit.hpp
 *  \brief   FlowUnit class declaration
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __FLOWUNIT_HEADER__
#define __FLOWUNIT_HEADER__

#include <math.h>

// OpenCAEPoroX header files
#include "OCPConst.hpp"
#include "OCPTable.hpp"
#include "OCPSATFunc.hpp"
#include "OptionalFeatures.hpp"
#include "ParamReservoir.hpp"
#include "OCP3PhaseFlow.hpp"

/// designed to deal with matters related to saturation table.
/// relative permeability, capillary pressure will be calculated here.
class FlowUnit
{
public:
    /// Default constructor.
    FlowUnit() = default;
    void Allocate(const USI& np) {
        kr.resize(np, 0);
        pc.resize(np, 0);
        dKrdS.resize(np * np, 0);
        dPcdS.resize(np * np, 0);
    }
    virtual void SetupOptionalFeatures(OptionalFeatures& optFeatures) = 0;
    virtual void
    SetupScale(const OCP_USI& bId, OCP_DBL& Swinout, const OCP_DBL& Pcowin) = 0;
    /// Pcow = Po - Pw
    virtual OCP_DBL GetPcowBySw(const OCP_DBL& sw)   = 0;
    virtual OCP_DBL GetSwByPcow(const OCP_DBL& pcow) = 0;
    /// Pcgo = Pg - Po
    virtual OCP_DBL GetPcgoBySg(const OCP_DBL& sg)   = 0;
    virtual OCP_DBL GetSgByPcgo(const OCP_DBL& pcgo) = 0;
    /// Pcgw = Pg - Pw
    virtual OCP_DBL GetSwByPcgw(const OCP_DBL& pcgw) = 0;
    /// Return the value of Scm
    virtual const vector<OCP_DBL>& GetScm() const = 0;

    /// Calculate relative permeability and capillary pressure.
    virtual void CalKrPc(const OCP_DBL* S_in, const OCP_USI& bId) = 0;

    /// Calculate derivatives of relative permeability and capillary pressure.
    virtual void CalKrPcFIM(const OCP_DBL* S_in, const OCP_USI& bId) = 0;

    OCP_DBL GetSwco() const { return Swco; };
    const vector<OCP_DBL>& GetKr()const { return kr; }
    const vector<OCP_DBL>& GetPc()const { return pc; }
    const vector<OCP_DBL>& GetdKrdS()const { return dKrdS; }
    const vector<OCP_DBL>& GetdPcdS()const { return dPcdS; }

protected:

    OCP_USI         bulkId; ///< index of work bulk

    vector<OCP_DBL> Scm;    ///< critical saturation when phase becomes mobile / immobile

    OCP_DBL         Swco;   ///< saturaion of connate water

    vector<OCP_DBL> kr;     ///< relative permeability of phase
    vector<OCP_DBL> pc;     ///< capillary pressure
    vector<OCP_DBL> dKrdS;  ///< dKr / dPc
    vector<OCP_DBL> dPcdS;  ///< dKr / dS

    vector<OCP_DBL> data;   ///< container to store the values of interpolation.
    vector<OCP_DBL> cdata;  ///< container to store the slopes of interpolation.
};

///////////////////////////////////////////////
// FlowUnit_W
///////////////////////////////////////////////

class FlowUnit_W : public FlowUnit
{
public:
    FlowUnit_W() = default;
    FlowUnit_W(const ParamReservoir& rs_param, const USI& i){ 
        Allocate(1);
        kr[0]    = 1;
        pc[0]    = 0;
        dKrdS[0] = 0;
        dPcdS[0] = 0;
    };
    void SetupOptionalFeatures(OptionalFeatures& optFeatures) override{};
    void
    SetupScale(const OCP_USI& bId, OCP_DBL& Swinout, const OCP_DBL& Pcowin) override{};
    void CalKrPc(const OCP_DBL* S_in, const OCP_USI& bId) override;
    void CalKrPcFIM(const OCP_DBL* S_in, const OCP_USI& bId) override;

    OCP_DBL GetPcowBySw(const OCP_DBL& sw) override { return 0.0; }
    OCP_DBL GetSwByPcow(const OCP_DBL& pcow) override { return 1.0; }
    OCP_DBL GetPcgoBySg(const OCP_DBL& sg) override { return 0.0; }
    OCP_DBL GetSgByPcgo(const OCP_DBL& pcgo) override { return 0.0; }
    OCP_DBL GetSwByPcgw(const OCP_DBL& pcgw) override { return 1.0; }

    const vector<OCP_DBL>& GetScm() const override
    {
        OCP_ABORT("Not Completed in this Condition!");
    }
};

///////////////////////////////////////////////
// FlowUnit_OW
///////////////////////////////////////////////

class FlowUnit_OW : public FlowUnit
{
public:
    FlowUnit_OW() = default;
    FlowUnit_OW(const ParamReservoir& rs_param, const USI& i);
    void SetupOptionalFeatures(OptionalFeatures& optFeatures) override{};
    void
    SetupScale(const OCP_USI& bId, OCP_DBL& Swinout, const OCP_DBL& Pcowin) override{};
    void CalKrPc(const OCP_DBL* S_in, const OCP_USI& bId) override;
    void CalKrPcFIM(const OCP_DBL* S_in, const OCP_USI& bId) override;
    OCP_DBL GetPcowBySw(const OCP_DBL& Sw) override { return SWOF.CalPcow(Sw); }
    OCP_DBL GetSwByPcow(const OCP_DBL& Pcow) override { return SWOF.CalSw(Pcow); }

    // useless
    OCP_DBL GetPcgoBySg(const OCP_DBL& sg) override { return 0; }
    OCP_DBL GetSgByPcgo(const OCP_DBL& pcgo) override { return 0; }
    OCP_DBL GetSwByPcgw(const OCP_DBL& pcgw) override { return 0; }

    const vector<OCP_DBL>& GetScm() const override
    {
        OCP_ABORT("Not Completed in this Condition!");
    }

protected:
    OCP_SWOF SWOF; ///< saturation table about water and oil.
};

///////////////////////////////////////////////
// FlowUnit_OG
///////////////////////////////////////////////

class FlowUnit_OG : public FlowUnit
{
public:
    FlowUnit_OG() = default;
    FlowUnit_OG(const ParamReservoir& rs_param, const USI& i);
    void SetupOptionalFeatures(OptionalFeatures& optFeatures) override{};
    void
    SetupScale(const OCP_USI& bId, OCP_DBL& Swinout, const OCP_DBL& Pcowin) override{};
    void    CalKrPc(const OCP_DBL* S_in, const OCP_USI& bId) override;
    void    CalKrPcFIM(const OCP_DBL* S_in, const OCP_USI& bId) override;
    OCP_DBL GetPcgoBySg(const OCP_DBL& Sg) override { return SGOF.CalPcgo(Sg);}
    OCP_DBL GetSgByPcgo(const OCP_DBL& Pcgo) override { return SGOF.CalSg(Pcgo); }

    // useless
    OCP_DBL GetPcowBySw(const OCP_DBL& sw) override { return 0; }
    OCP_DBL GetSwByPcow(const OCP_DBL& pcow) override { return 0; }
    OCP_DBL GetSwByPcgw(const OCP_DBL& pcgw) override { return 0; }

    const vector<OCP_DBL>& GetScm() const override
    {
        OCP_ABORT("Not Completed in this Condition!");
    }

protected:
    OCP_SGOF SGOF; ///< saturation table about gas and oil.
};

///////////////////////////////////////////////
// FlowUnit_OGW
///////////////////////////////////////////////

class FlowUnit_OGW : public FlowUnit
{
public:
    void SetupOptionalFeatures(OptionalFeatures& optFeatures) override final;
    void SetupScale(const OCP_USI& bId, OCP_DBL& Swinout, const OCP_DBL& Pcowin) override final;
    void CalKrPc(const OCP_DBL* S_in, const OCP_USI& bId) override final;
    void CalKrPcFIM(const OCP_DBL* S_in, const OCP_USI& bId) override final;

protected:
    virtual OCP_DBL GetKrgDer(const OCP_DBL& Sg, OCP_DBL& dKrgdSg) = 0;

protected:
    virtual void CalKrPc(const OCP_DBL* S_in) = 0;
    virtual void CalKrPcFIM(const OCP_DBL* S_in) = 0;
    OCP_DBL CalKro_Stone2(const OCP_DBL& krow,
                          const OCP_DBL& krog,
                          const OCP_DBL& krw,
                          const OCP_DBL& krg) const;

    OCP_DBL CalKro_Default(const OCP_DBL& Sg,
                           const OCP_DBL& Sw,
                           const OCP_DBL& krog,
                           const OCP_DBL& krow) const;

    const vector<OCP_DBL>& GetScm() const override { return Scm; }

protected:
    void AssinValueDer();
    void AssinValue();

protected:
    /// phase index
    enum phaseIndex {oIndex, gIndex, wIndex};
    enum p2p { oo, og, ow, go, gg, gw, wo, wg, ww };

    OCP3PhaseFlow  PF3;
    const OCP_BOOL newM{ true };

    /// oil relative permeability in the presence of connate water only, used in stone2
    OCP_DBL    krocw;

    // For scaling the water-oil capillary pressure curves
    ScalePcow* scalePcow;      ///< ptr to ScalePcow modules
    USI        spMethodIndex;  ///< index of scalePcow

    OCP_DBL    maxPcow;        ///< maximum Pcow
    OCP_DBL    minPcow;        ///< minimum Pcow
    // For miscible
    Miscible*  miscible;
    USI        mcMethodIndex;  ///< index of miscible curve
};

///////////////////////////////////////////////
// FlowUnit_OGW01
///////////////////////////////////////////////

class FlowUnit_OGW01 : public FlowUnit_OGW
{
public:
    FlowUnit_OGW01() = default;
    FlowUnit_OGW01(const ParamReservoir& rs_param, const USI& i);
    OCP_DBL GetPcowBySw(const OCP_DBL& Sw) override { return SWOF.CalPcow(Sw); }
    OCP_DBL GetSwByPcow(const OCP_DBL& Pcow) override { return SWOF.CalSw(Pcow); }

    OCP_DBL GetPcgoBySg(const OCP_DBL& Sg) override { return SGOF.CalPcgo(Sg); }
    OCP_DBL GetSgByPcgo(const OCP_DBL& Pcgo) override { return SGOF.CalSg(Pcgo); }
    OCP_DBL GetSwByPcgw(const OCP_DBL& pcgw) override { return SWPCGW.Eval_Inv(1, pcgw, 0); }

protected:
    OCP_DBL GetKrgDer(const OCP_DBL& Sg, OCP_DBL& dKrgdSg) override { return SGOF.CalKrg(Sg, dKrgdSg); }

protected:
    void CalKrPc(const OCP_DBL* S_in) override;
    void CalKrPcFIM(const OCP_DBL* S_in) override;
    OCP_DBL CalKro_Stone2Der(const OCP_DBL&  krow,
                             const OCP_DBL&  krog,
                             const OCP_DBL&  krw,
                             const OCP_DBL&  krg,
                             const OCP_DBL&  dkrwdSw,
                             const OCP_DBL&  dkrowdSw,
                             const OCP_DBL&  dkrgdSg,
                             const OCP_DBL&  dkrogdSg,
                             OCP_DBL& out_dkrodSw,
                             OCP_DBL& out_dkrodSg) const;
    OCP_DBL CalKro_DefaultDer(const OCP_DBL& Sg,
                              const OCP_DBL& Sw,
                              const OCP_DBL& krog,
                              const OCP_DBL& krow,
                              const OCP_DBL& dkrogSg,
                              const OCP_DBL& dkrowSw,
                              OCP_DBL&       dkroSg,
                              OCP_DBL&       dkroSw) const;
    void Generate_SWPCWG();

protected:
    OCP_SGOF SGOF;   ///< saturation table about gas and oil.
    OCP_SWOF SWOF;   ///< saturation table about water and oil.
    OCPTable SWPCGW; ///< auxiliary table: saturation of water vs. capillary
                     ///< pressure between water and gas.
};

///////////////////////////////////////////////
// FlowUnit_OGW01_Miscible
///////////////////////////////////////////////

class FlowUnit_OGW01_Miscible : public FlowUnit_OGW01
{
public:
    FlowUnit_OGW01_Miscible(const ParamReservoir& rs_param, const USI& i)
        : FlowUnit_OGW01(rs_param, i)
    {
        // gas is moveable all the time
        maxPcow = SWOF.GetMaxPc();
        minPcow = SWOF.GetMinPc();
    }

protected:
    void CalKrPc(const OCP_DBL* S_in) override;
    void CalKrPcFIM(const OCP_DBL* S_in) override;

protected:

    OCP_DBL Fk;     ///< The relative permeability interpolation parameter
    OCP_DBL Fp;     ///< The capillary pressure interpolation parameter
};

///////////////////////////////////////////////
// FlowUnit_OGW02
///////////////////////////////////////////////

class FlowUnit_OGW02 : public FlowUnit_OGW
{
public:
    FlowUnit_OGW02() = default;
    FlowUnit_OGW02(const ParamReservoir& rs_param, const USI& i);
    OCP_DBL GetPcowBySw(const OCP_DBL& sw) override { return SWFN.CalPcow(sw);}
    OCP_DBL GetSwByPcow(const OCP_DBL& pcow) override
    {
        return SWFN.CalSw(pcow);
    }
    OCP_DBL GetPcgoBySg(const OCP_DBL& sg) override { return SGFN.CalPcgo(sg); }
    OCP_DBL GetSgByPcgo(const OCP_DBL& pcgo) override { return SGFN.CalSg(pcgo); }
    OCP_DBL GetSwByPcgw(const OCP_DBL& pcgw) override
    {
        return SWPCGW.Eval_Inv(1, pcgw, 0);
    }
    void Generate_SWPCWG();

protected:
    OCP_DBL GetKrgDer(const OCP_DBL& Sg, OCP_DBL& dKrgdSg) override { OCP_ABORT("NOT COMPLETED!"); }

protected:
    void CalKrPc(const OCP_DBL* S_in) override;
    void CalKrPcFIM(const OCP_DBL* S_in) override;
    OCP_DBL CalKro_Stone2Der(const OCP_DBL& krow,
        const OCP_DBL& krog,
        const OCP_DBL& krw,
        const OCP_DBL& krg,
        const OCP_DBL& dkrwdSw,
        const OCP_DBL& dkrowdSo,
        const OCP_DBL& dkrgdSg,
        const OCP_DBL& dkrogdSo,
        OCP_DBL& out_dkrodSo) const;
    OCP_DBL CalKro_DefaultDer(const OCP_DBL& Sg,
        const OCP_DBL& Sw,
        const OCP_DBL& krog,
        const OCP_DBL& krow,
        const OCP_DBL& dkrogSo,
        const OCP_DBL& dkrowSo,
        OCP_DBL& dkroSo) const;

protected:
    OCP_SWFN SWFN;
    OCP_SGFN SGFN;
    OCP_SOF3 SOF3;
    OCPTable SWPCGW; ///< auxiliary table: saturation of water vs. capillary
                     ///< pressure between water and gas.
};

#endif /* end if __FLOWUNIT_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/05/2022      Format file                          */
/*----------------------------------------------------------------------------*/
/*! \file    BulkConnTrans.hpp
 *  \brief   BulkConnTrans class declaration
 *  \author  Shizhe Li
 *  \date    Aug/24/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __BULKCONNAREA_HEADER__
#define __BULKCONNAREA_HEADER__


// OpenCAEPoroX header files
#include "BulkConnVarSet.hpp"
#include "Bulk.hpp"


using namespace std;


class BulkConnTransMethod
{
public:
    BulkConnTransMethod() = default;
    virtual void CalTrans(BulkConnPair& bp, const Bulk& bk) = 0;
};


class BulkConnTransMethod01 : public BulkConnTransMethod
{
public:
    BulkConnTransMethod01() = default;
    void CalTrans(BulkConnPair& bp, const Bulk& bk) override;
};



class BulkConnTrans
{
public:
    BulkConnTrans() = default;
    void Setup();
    void CalTrans(BulkConnPair& bp, const Bulk& bk) { bcaM->CalTrans(bp, bk); }

protected:
    /// method for area calculation of bulk connection
    BulkConnTransMethod* bcaM;
};


#endif /* end if __BulkConnTrans_HEADER__ */

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Aug/24/2023      Create file                          */
/*----------------------------------------------------------------------------*/

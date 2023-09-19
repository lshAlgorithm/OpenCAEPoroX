/*! \file    OCPConst.hpp
 *  \brief   Definition of build-in datatypes and consts
 *  \author  Shizhe Li
 *  \date    Oct/01/2021
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#ifndef __OPENCAEPORO_CONSTS_HEADER__
#define __OPENCAEPORO_CONSTS_HEADER__

// Standard header files
#include <iostream>
#include <math.h>

// OpenCAEPoroX header files
#include "UtilError.hpp"

// Build-in data type
typedef unsigned int       USI;      ///< Generic unsigned integer
typedef unsigned long long OCP_ULL;  ///< Long long unsigned integer
typedef unsigned int       OCP_USI;  ///< Long unsigned integer
typedef int                INT;      ///< Generic signed integer
typedef int                OCP_INT;  ///< Long integer
typedef double             OCP_DBL;  ///< Double precision
typedef float              OCP_SIN;  ///< Single precision
typedef unsigned int       OCP_BOOL; ///< OCP_BOOL in OCP
typedef char               OCP_CHAR; ///< Char

// General error type
const int OCP_SUCCESS         = 0;    ///< Finish without trouble
const int OCP_ERROR_NUM_INPUT = -1;   ///< Wrong number of input param
const int OCP_ERROR           = -100; ///< Unidentified error

// General consts
const OCP_DBL  GAS_CONSTANT = 10.73159;    ///< Gas Constant
const OCP_DBL  TINY         = 1E-8;        ///< Small constant
const OCP_DBL  OCP_HUGE     = 1E16;        ///< Huge constant
const OCP_DBL  PI           = 3.141592653; ///< Pi
const OCP_BOOL OCP_TRUE     = 1;
const OCP_BOOL OCP_FALSE    = 0;

// Control consts
const OCP_DBL MAX_TIME_STEP     = 365.0; ///< Maximal time stepsize
const OCP_DBL MIN_TIME_STEP     = 0.01;  ///< Minimal time stepsize
const OCP_DBL MIN_TIME_CURSTEP  = 1E-6;  ///< Minimal time stepsize of current step ???
const OCP_DBL TIME_STEP_CUT     = 0.5;   ///< Time stepsize cut ratio
const OCP_DBL TIME_STEP_AMPLIFY = 2.0;   ///< Time stepsize amplification ratio
const OCP_DBL MAX_VOLUME_ERR    = 0.01;  ///< Maximal volume error
const OCP_DBL MAX_DP_LIMIT      = 200;   ///< Maximal pressure change
const OCP_DBL MAX_DS_LIMIT      = 0.1;   ///< Maximal saturation change
const OCP_DBL TARGET_DP         = 50;    ///< Target pressure change
const OCP_DBL TARGET_DS         = 0.01;  ///< Target saturation change

// Physical consts
const OCP_DBL GRAVITY_FACTOR  = 0.00694444; ///< 0.00694444 ft2 psi / lb
const OCP_DBL RHOW_STD        = 62.3664;    ///< Water density at surface cond: lb/ft3
const OCP_DBL RHOAIR_STD      = 0.076362;   ///< Air density at surface cond : lb/ft3
const OCP_DBL PRESSURE_STD    = 14.7;       ///< 14.6959 psia = 1 atm
const OCP_DBL TEMPERATURE_STD = 60;         ///< Standard temperature: F

// Unit conversion consts
const OCP_DBL CONV1 = 5.61458;               ///< 1 bbl = CONV1 ft3
const OCP_DBL CONV2 = 1.12712E-3;            ///< Darcy constant in field unit
const OCP_DBL CONV3 = 0.45359237;            ///< 1 lb = CONV3 kg
const OCP_DBL CONV4 = 0.02831685;            ///< 1 ft3 = CONV4 m3
const OCP_DBL CONV5 = 459.67;                ///< 0 F = CONV5 R
const OCP_DBL CONV6 = 778.172448;            ///<
const OCP_DBL CONV7 = CONV3 / (CONV4 * 1E3); ///< lbm/ft3 -> gm-M/cc


enum class OCPModel
{
    /// none
    none,
    /// thermal model
    isothermal,
    /// thermal model
    thermal
};

// Solution methods
const USI IMPEC = 1; ///< Solution method = IMPEC
const USI FIM   = 2; ///< Solution method = FIM
const USI AIMc  = 3; ///< Adaptive implicit ---- Collins


enum class ConnDirect
{
    /// none
    n,
    /// x-direction
    x,
    /// y-direction
    y,
    /// z-direction
    z,
    /// martrix -> fracture
    mf,
    /// fracture -> matrix
    fm,
    /// (i,j,k) -> (i+1,j,k),  structural grid only
    xp,
    /// (i,j,k) -> (i-1,j,k),  structural grid only
    xm,
    /// (i,j,k) -> (i,j+1,k),  structural grid only
    yp,
    /// (i,j,k) -> (i,j-1,k),  structural grid only
    ym,
    /// (i,j,k) -> (i,j,k+1),  structural grid only
    zp,
    /// (i,j,k) -> (i,j,k-1),  structural grid only
    zm,
};


// Parallel Module
const USI MASTER_PROCESS = 0; ///< master process


/**
 * \brief Print level for all subroutines -- not including DEBUG output
 */
#define PRINT_NONE              0  /**< silent: no printout at all */
#define PRINT_MIN               1  /**< quiet:  print error, important warnings */
#define PRINT_SOME              2  /**< some:   print less important warnings */
#define PRINT_MORE              4  /**< more:   print some useful debug info */
#define PRINT_MOST              8  /**< most:   maximal printouts, no files */
#define PRINT_ALL              10  /**< all:    all printouts, including files */

/**
 * \brief Definition of max, min, abs
 */
#define OCP_MAX(a,b) (((a)>(b))?(a):(b))     /**< bigger one in a and b */
#define OCP_MIN(a,b) (((a)<(b))?(a):(b))     /**< smaller one in a and b */
#define OCP_ABS(a)   (((a)>=0.0)?(a):-(a))   /**< absolute value of a */

#endif // __OPENCAEPORO_CONSTS_HEADER__

/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Oct/01/2021      Create file                          */
/*  Chensong Zhang      Oct/15/2021      Format file                          */
/*  Chensong Zhang      Oct/27/2021      Unify error check                    */
/*  Chensong Zhang      Jan/16/2022      Update Doxygen                       */
/*  Chensong Zhang      Sep/21/2022      Add error messages                   */
/*----------------------------------------------------------------------------*/
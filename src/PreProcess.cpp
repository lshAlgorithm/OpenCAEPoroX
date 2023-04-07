/*! \file    PreProcess.cpp
 *  \brief   PreProcess for OpenCAEPoroX simulator
 *  \author  Shizhe Li
 *  \date    Feb/15/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "PreProcess.hpp"


PreProcess::PreProcess(const string& myFile, const OCP_INT& myRank, MPI_Comm comm)
{  
    if (myRank == MASTER_PROCESS) {   
        GetFile(myFile);
        preParamGridWell.InputFile(filename, workdir);
        preParamGridWell.Setup();
    }

    partition.InitMPI(comm);
    partition.SetPartition(preParamGridWell);
    partition.SetDistribution();
    domain.Setup(partition);
}


/// Get workDir and fileName from inputFile.
void PreProcess::GetFile(const string& myFile)
{
    inputFile = myFile;

#if defined(_CONSOLE) || defined(_WIN32) || defined(_WIN64)
    // for Window file system
    OCP_INT pos = inputFile.find_last_of('\\') + 1;
    workdir = inputFile.substr(0, pos);
    filename = inputFile.substr(pos, inputFile.size() - pos);
#else
    // for Linux and Mac OSX file system
    OCP_INT pos = inputFile.find_last_of('/') + 1;
    workdir = inputFile.substr(0, pos);
    filename = inputFile.substr(pos, inputFile.size() - pos);
#endif
}


/*----------------------------------------------------------------------------*/
/*  Brief Change History of This File                                         */
/*----------------------------------------------------------------------------*/
/*  Author              Date             Actions                              */
/*----------------------------------------------------------------------------*/
/*  Shizhe Li           Feb/15/2023      Create file                          */
/*----------------------------------------------------------------------------*/
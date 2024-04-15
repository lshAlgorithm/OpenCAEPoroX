/*! \file    Partition.cpp
 *  \brief   Partition for OpenCAEPoroX simulator
 *  \author  Shizhe Li
 *  \date    Feb/28/2023
 *
 *-----------------------------------------------------------------------------------
 *  Copyright (C) 2021--present by the OpenCAEPoroX team. All rights reserved.
 *  Released under the terms of the GNU Lesser General Public License 3.0 or later.
 *-----------------------------------------------------------------------------------
 */

#include "Domain.hpp"



void Domain::Setup(const Partition& part, const PreParamGridWell& gridwell)
{
	global_comm  = part.myComm;	
	numproc      = part.numproc;
	myrank       = part.myrank;
	MPI_Comm_group(global_comm, &global_group);

	if (numproc == 1) {
		numElementTotal = part.numElementTotal;
		numElementLocal = numElementTotal;
		numWellTotal    = part.numWellTotal;
		numGridInterior = numElementTotal - numWellTotal;
		numGridGhost    = 0;
		numGridLocal    = numGridInterior;

		numSendProc     = 0;
		numRecvProc     = 0;
		grid.resize(numGridInterior);
		for (OCP_USI n = 0; n < numGridInterior; n++) {
			grid[n] = n;
			init_global_to_local.insert(make_pair(n, n));
		}
		well.resize(numWellTotal);
		for (OCP_USI w = 0; w < numWellTotal; w++) {
			well[w] = w;
		}
		return;
	}

	if (CURRENT_RANK == MASTER_PROCESS) {
		OCP_INFO("Set Domain -- begin");
	}


	// Setup Domain
	elementCSR.swap(part.elementCSR);	
	numElementTotal = part.numElementTotal;
	numWellTotal    = part.numWellTotal;
	numElementLocal = 0;

	for (const auto& e : elementCSR) {
		numElementLocal += e.second[0];
	}

	// Traverse elementCSR in ascending order of process number
	OCP_ULL  global_well_start = numElementTotal - numWellTotal;
	OCP_USI  localIndex        = 0;
	vector<set<OCP_ULL>> ghostElement;
	grid.reserve(numElementLocal * 1.5); // preserved space
	for (const auto& e : elementCSR) {
		const auto&  ev           = e.second;
		const idx_t* my_vtx       = &ev[2];
		const idx_t* my_xadj      = &ev[2 + ev[0]];
		const idx_t* my_edge      = &ev[2 + ev[0] + (ev[0] + 1)];
		const idx_t* my_edge_proc = &ev[2 + ev[0] + (ev[0] + 1) + ev[1]];
		for (USI i = 0; i < ev[0]; i++) {
			if (my_vtx[i] >= global_well_start) {
				// well
				well.push_back(my_vtx[i] - global_well_start);
				continue;
			}
			grid.push_back(my_vtx[i]);
			init_global_to_local.insert(make_pair(static_cast<OCP_ULL>(my_vtx[i]), localIndex));
			for (USI j = my_xadj[i]; j < my_xadj[i + 1]; j++) {
				if (my_edge_proc[j] != myrank) {
					// current interior grid is also ghost grid of other process
					USI s = 0;
					for (s = 0; s < send_element_loc.size(); s++) {
						if (my_edge_proc[j] == send_element_loc[s][0]) {
							if (localIndex != send_element_loc[s].back()) {
								send_element_loc[s].push_back(localIndex);
							}
							ghostElement[s].insert(static_cast<OCP_ULL>(my_edge[j]));
							break;
						}
					}
					if (s == send_element_loc.size()) {
						send_element_loc.push_back(vector<OCP_USI>{static_cast<OCP_USI>(my_edge_proc[j]), localIndex});
						ghostElement.push_back(set<OCP_ULL>{static_cast<OCP_ULL>(my_edge[j])});
					}
				}
			}
			localIndex++;
		}
	}

	numGridInterior = init_global_to_local.size();
	numWellLocal    = well.size();
	OCP_ASSERT(numGridInterior == numElementLocal - numWellLocal, "");

	recv_element_loc.resize(send_element_loc.size());
	for (USI i = 0; i < send_element_loc.size(); i++) {
		recv_element_loc[i].reserve(3);
		recv_element_loc[i].push_back(send_element_loc[i][0]);
	}

	numGridGhost = 0;
	localIndex = init_global_to_local.size();
	USI sIter = 0;
	for (const auto& g : ghostElement) {
		numGridGhost += g.size();
		recv_element_loc[sIter].push_back(localIndex);
		for (const auto& g1 : g) {
			grid.push_back(g1);
			init_global_to_local.insert(make_pair(g1, localIndex));
			localIndex++;
		}
		recv_element_loc[sIter].push_back(localIndex);
		sIter++;
	}
	numGridLocal = numGridInterior + numGridGhost;
	numSendProc  = send_element_loc.size();
	numRecvProc  = recv_element_loc.size();
	send_request.resize(numSendProc);
	recv_request.resize(numRecvProc);

	grid.shrink_to_fit();

	//////////////////////////////////////////////////////////////
	// Output partition information
	//////////////////////////////////////////////////////////////

	if (false) {
		ofstream myFile;
		myFile.open("test/process" + to_string(myrank) + ".txt");
		ios::sync_with_stdio(false);
		myFile.tie(0);

		for (const auto& e : elementCSR) {
			const auto& ev = e.second;
			// process, num grid, num edges
			myFile << setw(8) << e.first << setw(8) << ev[0] << setw(8) << ev[1] << endl;
			const idx_t* my_vtx       = &ev[2];
			const idx_t* my_xadj      = &ev[2 + ev[0]];
			const idx_t* my_edge      = &ev[2 + ev[0] + ev[0] + 1];
			const idx_t* my_edge_proc = &ev[2 + ev[0] + ev[0] + 1 + ev[1]];
			// vertex
			for (int i = 0; i < ev[0]; i++) {
				myFile << setw(8) << my_vtx[i];
			}
			myFile << endl;
			// vertex
			for (int i = 0; i < ev[0]; i++) {
				for (int j = my_xadj[i]; j < my_xadj[i + 1]; j++) {
					myFile << setw(8) << my_vtx[i];
				}
			}
			myFile << endl;
			// edges
			for (int i = 0; i < ev[1]; i++) {
				myFile << setw(8) << my_edge[i];
			}
			myFile << endl;
			// process
			for (int i = 0; i < ev[1]; i++) {
				myFile << setw(8) << my_edge_proc[i];
			}
			myFile << endl << endl << endl;
		}

		myFile << "init_global_to_local" << endl;
		for (const auto& m : init_global_to_local) {
			myFile << setw(8) << m.first;
		}
		myFile << endl;
		for (const auto& m : init_global_to_local) {
			myFile << setw(8) << m.second;
		}
		myFile << endl << endl;
		myFile << "local_to_init_global" << endl;
		for (int i = 0; i < grid.size(); i++) {
			myFile << setw(8) << i;
		}
		myFile << endl;
		for (const auto& e : grid) {
			myFile << setw(8) << e;
		}
		myFile << endl << endl << endl << "send" << endl;
		for (const auto& s : send_element_loc) {
			for (const auto& s1 : s) {
				myFile << setw(8) << s1;
			}
			myFile << endl;
		}
		myFile << endl << endl << endl << "recv" << endl;
		for (const auto& r : recv_element_loc) {
			for (const auto& r1 : r) {
				myFile << setw(8) << r1;
			}
			myFile << endl;
		}
		myFile << endl << endl << endl << "well" << endl;
		for (USI w = 0; w < well.size(); w++) {
			myFile << setw(8) << well[w];
		}
		myFile << endl << endl << endl;
		myFile << "Grid Num" << endl;
		myFile << setw(8) << numElementLocal << setw(8) << numGridInterior
			<< setw(8) << numWellLocal << setw(8) << numGridGhost << endl;
		myFile.close();
	}
	//////////////////////////////////////////////////////////////


	if (CURRENT_RANK == MASTER_PROCESS) {
		OCP_INFO("Set Domain -- end");
	}
}


void Domain::SetLocalComm(const vector<OCP_USI>& bIds)
{
	//if (MPI_GROUP_NULL != loc_group) MPI_Group_free(&loc_group);
	//if (MPI_COMM_NULL != loc_comm) MPI_Comm_free(&loc_comm);

	loc_group_rank.clear();
	for (const auto& b : bIds) {
		if (b < numGridInterior) {

		}
		else {
			for (const auto& r : recv_element_loc) {
				if (b >= r[1] && b < r[2]) {
					loc_group_rank.insert(r[0]);
					break;
				}
			}
		}

	}
	vector<INT> tmp_rank;
	for (const auto& r : loc_group_rank) {
		tmp_rank.push_back(r);
	}
	MPI_Group_incl(global_group, tmp_rank.size(), tmp_rank.data(), &loc_group);
	MPI_Comm_create(MPI_COMM_WORLD, loc_group, &loc_comm);
}


OCP_INT Domain::GetPerfLocation(const OCP_USI& wId, const USI& p) const
{
	for (const auto& w : wellWPB) {
		if (w[0] == wId) {
			for (USI i = 1; i < w.size(); i += 2) {
				if (w[i] == p) {
					return w[i + 1];
				}
			}
		}
	}
	return -1;
}


USI Domain::GetPerfNum(const OCP_USI& wId) const
{
	for (const auto& w : wellWPB) {
		if (w[0] == wId) {
			return (w.size() - 1) / 2;
		}
	}
	OCP_ABORT("WRONG WELL SETUP!");
}


const vector<OCP_ULL>* Domain::CalGlobalIndex() const
{
	global_index.resize(numGridLocal + numActWellLocal);

	const OCP_ULL numElementLoc = numGridInterior + numActWellLocal;
	OCP_ULL       global_begin;
	OCP_ULL       global_end;

	GetWallTime timer;
	timer.Start();

	MPI_Scan(&numElementLoc, &global_end, 1, OCPMPI_ULL, MPI_SUM, global_comm);

	OCPTIME_COMM_COLLECTIVE += timer.Stop();

	global_begin = global_end - numElementLoc;
	global_end   = global_end - 1;

	// Get Interior grid's global index
	for (OCP_USI n = 0; n < numElementLoc; n++)
		global_index[n] = n + global_begin;

	timer.Start();

	// Get Ghost grid's global index by communication	
	for (USI i = 0; i < numRecvProc; i++) {
		const auto& rel = recv_element_loc[i];
		const auto  bId = rel[1] + numActWellLocal;
		MPI_Irecv(&global_index[bId], rel[2] - rel[1], OCPMPI_ULL, rel[0], 0, global_comm, &recv_request[i]);
	}
	vector<vector<OCP_ULL>> send_buffer(numSendProc);
	for (USI i = 0; i < numSendProc; i++) {
		const auto& sel = send_element_loc[i];
		auto&       s   = send_buffer[i];
		s.resize(sel.size());
		s[0] = sel[0];
		for (USI j = 1; j < sel.size(); j++) {
			s[j] = global_index[sel[j]];
		}
		MPI_Isend(s.data() + 1, s.size() - 1, OCPMPI_ULL, s[0], 0, global_comm, &send_request[i]);
	}

	MPI_Waitall(numRecvProc, recv_request.data(), MPI_STATUS_IGNORE);
	MPI_Waitall(numSendProc, send_request.data(), MPI_STATUS_IGNORE);

	OCPTIME_COMM_P2P += timer.Stop();

	return &global_index;
}




 /*----------------------------------------------------------------------------*/
 /*  Brief Change History of This File                                         */
 /*----------------------------------------------------------------------------*/
 /*  Author              Date             Actions                              */
 /*----------------------------------------------------------------------------*/
 /*  Shizhe Li           Feb/28/2023      Create file                          */
 /*----------------------------------------------------------------------------*/
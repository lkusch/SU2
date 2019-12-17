/*!
 * \file CDiscAdjFEAVariable.cpp
 * \brief Definition of the variables for FEM adjoint elastic structural problems.
 * \author Ruben Sanchez
 * \version 7.0.0 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation 
 * (http://su2foundation.org)
 *
 * Copyright 2012-2019, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */


#include "../../include/variables/CDiscAdjFEABoundVariable.hpp"

CDiscAdjFEABoundVariable::CDiscAdjFEABoundVariable(const su2double *disp, const su2double *vel,
  const su2double *accel, unsigned long npoint, unsigned long ndim, unsigned long nvar, bool unsteady, CConfig *config) :
    CDiscAdjFEAVariable(disp, vel, accel, npoint, ndim, nvar, unsteady, config) {

  VertexMap.Reset(nPoint);
}

void CDiscAdjFEABoundVariable::AllocateBoundaryVariables(CConfig *config) {

  if (VertexMap.GetIsValid()) return; // nothing to do

  /*--- Count number of vertices and build map ---*/

  unsigned long nBoundPt = VertexMap.Build();

  /*--- Allocate ---*/

  FlowTraction_Sens.resize(nBoundPt,nDim) = su2double(0.0);
  SourceTerm_DispAdjoint.resize(nBoundPt,nDim) = su2double(0.0);

}
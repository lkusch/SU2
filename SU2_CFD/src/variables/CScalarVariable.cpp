/*!
 * \file CScalarVariable.cpp
 * \brief Definition of the scalar equation variables at each vertex.
 * \author D. Mayer, T. Economon
 * \version 7.1.0 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2020, SU2 Contributors (cf. AUTHORS.md)
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

#include "../../include/variables/CScalarVariable.hpp"

CScalarVariable::CScalarVariable(unsigned long npoint, unsigned long ndim, unsigned long nvar, CConfig *config) : CVariable(npoint, ndim, nvar, config) {
    
  /*--- Gradient related fields ---*/
  
  Gradient.resize(nPoint,nVar,nDim,0.0);
  
  if (config->GetKind_Gradient_Method() == WEIGHTED_LEAST_SQUARES) {
    Rmatrix.resize(nPoint,nDim,nDim,0.0);
  }
  
  /*--- Allocate residual structures ---*/
  
  Res_TruncError.resize(nPoint,nVar) = su2double(0.0);
  
  /*--- Always allocate the slope limiter, and the auxiliar
   variables (check the logic - JST with 2nd order Turb model) ---*/
  
  Limiter.resize(nPoint,nVar) = su2double(0.0);
  Solution_Max.resize(nPoint,nVar) = su2double(0.0);
  Solution_Min.resize(nPoint,nVar) = su2double(0.0);
  
  Delta_Time.resize(nPoint) = su2double(0.0);

   /* Under-relaxation parameter. */
  UnderRelaxation.resize(nPoint) = su2double(1.0);

  /*--- Allocate space for the mass diffusivity. ---*/
  
  Diffusivity.resize(nPoint,nVar) = su2double(0.0);
  
  /*--- If axisymmetric and viscous, we need an auxiliary gradient. ---*/
  
  if (config->GetAxisymmetric() && config->GetViscous()) {
    AuxVar.resize(nPoint,nVar)=su2double(0.0); //nijso: nvar is correct? do we use this?
    Grad_AuxVar.resize(nPoint,nVar,nDim);
  }

}
/*!
 * \file solver_adjoint_discrete.cpp
 * \brief Main subroutines for solving the discrete adjoint problem.
 * \author T. Albring
 * \version 4.0.0 "Cardinal"
 *
 * SU2 Lead Developers: Dr. Francisco Palacios (francisco.palacios@boeing.com).
 *                      Dr. Thomas D. Economon (economon@stanford.edu).
 *
 * SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
 *                 Prof. Piero Colonna's group at Delft University of Technology.
 *                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *                 Prof. Rafael Palacios' group at Imperial College London.
 *
 * Copyright (C) 2012-2015 SU2, the open-source CFD code.
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

#include "../include/solver_structure.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>

CDiscAdjSolver::CDiscAdjSolver(void) : CSolver (){

}

CDiscAdjSolver::CDiscAdjSolver(CGeometry *geometry, CConfig *config){

}

CDiscAdjSolver::CDiscAdjSolver(CGeometry *geometry, CConfig *config, CSolver *direct_solver, unsigned short Kind_Solver, unsigned short iMesh){
  unsigned short iVar, iMarker, iDim, nZone;
  unsigned long jVertex;

  bool restart = config->GetRestart();

  unsigned long iVertex, iPoint, index;
  string text_line, mesh_filename;
  ifstream restart_file;
  string filename, AdjExt;
  bool compressible = (config->GetKind_Regime() == COMPRESSIBLE);
  bool incompressible = (config->GetKind_Regime() == INCOMPRESSIBLE);
  bool freesurface = (config->GetKind_Regime() == FREESURFACE);
  su2double dull_val;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  nZone = geometry->GetnZone();
  nVar = direct_solver->GetnVar();
  nDim = geometry->GetnDim();


  /*-- Store some information about direct solver ---*/
  this->KindDirect_Solver = Kind_Solver;
  this->direct_solver = direct_solver;


  nMarker      = config->GetnMarker_All();
  nPoint       = geometry->GetnPoint();
  nPointDomain = geometry->GetnPointDomain();

  /*--- Allocate the node variables ---*/

  node = new CVariable*[nPoint];

  /*--- Define some auxiliary vectors related to the residual ---*/

  Residual      = new su2double[nVar];         for (iVar = 0; iVar < nVar; iVar++) Residual[iVar]      = 1.0;
  Residual_RMS  = new su2double[nVar];         for (iVar = 0; iVar < nVar; iVar++) Residual_RMS[iVar]  = 1.0;
  Residual_Max  = new su2double[nVar];         for (iVar = 0; iVar < nVar; iVar++) Residual_Max[iVar]  = 1.0;

  /*--- Define some structures for locating max residuals ---*/

  Point_Max     = new unsigned long[nVar];  for (iVar = 0; iVar < nVar; iVar++) Point_Max[iVar]     = 0;
  Point_Max_Coord = new su2double*[nVar];
  for (iVar = 0; iVar < nVar; iVar++) {
    Point_Max_Coord[iVar] = new su2double[nDim];
    for (iDim = 0; iDim < nDim; iDim++) Point_Max_Coord[iVar][iDim] = 0.0;
  }

  /*--- Define some auxiliary vectors related to the solution ---*/

  Solution   = new su2double[nVar];

  for (iVar = 0; iVar < nVar; iVar++) Solution[iVar]   = 0.0;

  /*--- Sensitivity definition and coefficient in all the markers ---*/


  machp=new su2double[4];
  points=new su2double[4];
  weights=new su2double[4];
  sigma=sqrt(0.0001);
  mu=0.8;
  points[0]=-1.65068012;
  points[1]=-0.52464762;
  points[2]=0.52464762;
  points[3]=1.65068012;
  weights[0]=0.08131284;
  weights[1]=0.80491409;
  weights[2]=0.80491409;
  weights[3]=0.08131284;
  for (unsigned short numQuad=0; numQuad<4;numQuad++){
      machp[numQuad]=sqrt(2)*sigma*points[numQuad]+mu;
  }


  rkStore=new su2double* [config->GetLBFGSNum()];
  dukStore=new su2double* [config->GetLBFGSNum()];
  CSensitivity = new su2double* [nMarker];
  CSensitivityOld = new su2double* [nMarker];
  LagrangeSens=new su2double* [nMarker];
  ExpCSensitivityOld = new su2double* [nMarker];
  ExpLagrangeSens=new su2double* [nMarker];
 // LagrangeSensOld=new su2double* [nMarker];
//  LagrangeSensSep= new su2double*[3];
  ProjectedSens=new su2double[38];
  ProjectedGradient=new su2double[38];
  ProjectedSensOld=new su2double[38];
  DesignVar=new su2double[38];
 // DesignVarOld=new su2double[38];
  DesignVarUpdate=new su2double[38];
  DesignVarUpdateSave=new su2double[38];
  DesignVarUpdateReal=new su2double[38];
  TotalIterations=0;
  for (iMarker=0; iMarker<38; iMarker++){
      DesignVar[iMarker]=0.0;
   //   DesignVarOld[iMarker]=0.0;
      DesignVarUpdate[iMarker]=0.0;
      DesignVarUpdateSave[iMarker]=0.0;
      DesignVarUpdateReal[iMarker]=0.0;
  }
  rho=0.01;
  normyold=1;
  BFGSCount=0;
  Constraint_Save=0.0;
  ExpConstraint_Save=0.0;
  Constraint_Old=0.0;
//  Hess=new su2double*[geometry->nVertex[0]];
  Hess=new su2double*[38];
  Bess=new su2double*[38];
//  yCoordNew=new su2double[geometry->nVertex[0]];
//  yCoordOld=new su2double[geometry->nVertex[0]];
  UpdateSens=new su2double[geometry->nVertex[0]];

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
      CSensitivity[iMarker]        = new su2double [geometry->nVertex[iMarker]];
      CSensitivityOld[iMarker]        = new su2double [geometry->nVertex[iMarker]];
      LagrangeSens[iMarker]        = new su2double [geometry->nVertex[iMarker]];
      ExpCSensitivityOld[iMarker]        = new su2double [geometry->nVertex[iMarker]];
      ExpLagrangeSens[iMarker]        = new su2double [geometry->nVertex[iMarker]];
    //  LagrangeSensOld[iMarker]        = new su2double [geometry->nVertex[iMarker]];
   //   if(iMarker<3) LagrangeSensSep[iMarker] = new su2double [geometry->nVertex[0]];
  }
  for (iMarker = 0; iMarker < config->GetLBFGSNum(); iMarker++) {
      rkStore[iMarker]=new su2double[38];
      dukStore[iMarker]=new su2double[38];
  }

 /* for (iVertex = 0; iVertex < geometry->nVertex[0]; iVertex++) {
     Hess[iVertex]= new su2double [geometry->nVertex[0]];
  }*/

  for (iVertex = 0; iVertex < 38; iVertex++) {
     Hess[iVertex]= new su2double [38];
     Bess[iVertex]= new su2double [38];
  }

  for (iVertex = 0; iVertex < geometry->nVertex[0]; iVertex++) {
/*      for (jVertex = 0; jVertex < geometry->nVertex[0]; jVertex++) {
         Hess[iVertex][jVertex]= 0.0;
      }*/
  //    Hess[iVertex][iVertex]=1.0;
      UpdateSens[iVertex]=0;
  }

  for (iVertex = 0; iVertex < 38; iVertex++) {
      for (jVertex = 0; jVertex < 38; jVertex++) {
         Hess[iVertex][jVertex]= 0.0;
         Bess[iVertex][jVertex]= 0.0;
      }
      Hess[iVertex][iVertex]=1.0;
      Bess[iVertex][iVertex]=1.0;
      if(config->GetHInit()){
          Hess[iVertex][iVertex]=config->GetHScale();
          Bess[iVertex][iVertex]=1.0/config->GetHScale();
      }
  }
  Sens_Geo  = new su2double[nMarker];
  Sens_Mach = new su2double[nMarker];
  Sens_AoA  = new su2double[nMarker];
  Sens_Press = new su2double[nMarker];
  Sens_Temp  = new su2double[nMarker];
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
      Sens_Geo[iMarker]  = 0.0;
      Sens_Mach[iMarker] = 0.0;
      Sens_AoA[iMarker]  = 0.0;
      Sens_Press[iMarker] = 0.0;
      Sens_Temp[iMarker]  = 0.0;
      for (iVertex = 0; iVertex < geometry->nVertex[iMarker]; iVertex++){
          CSensitivity[iMarker][iVertex] = 0.0;
          CSensitivityOld[iMarker][iVertex] = 0.0;
      //    LagrangeSensOld[iMarker][iVertex] = 0.0;
          ExpCSensitivityOld[iMarker][iVertex] = 0.0;
          ExpLagrangeSens[iMarker][iVertex] = 0.0;
          LagrangeSens[iMarker][iVertex] = 0.0;
      }
  }

  /*--- Check for a restart and set up the variables at each node
   appropriately. Coarse multigrid levels will be intitially set to
   the farfield values bc the solver will immediately interpolate
   the solution from the finest mesh to the coarser levels. ---*/
  if (!restart || (iMesh != MESH_0)) {

    /*--- Restart the solution from zero ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++)
      node[iPoint] = new CDiscAdjVariable(Solution, nDim, nVar, config);

  }
  else {

    /*--- Restart the solution from file information ---*/
    mesh_filename = config->GetSolution_AdjFileName();
    filename = config->GetObjFunc_Extension(mesh_filename);

    restart_file.open(filename.data(), ios::in);

    /*--- In case there is no file ---*/
    if (restart_file.fail()) {
      if (rank == MASTER_NODE)
        cout << "There is no adjoint restart file!! " << filename.data() << "."<< endl;
      exit(EXIT_FAILURE);
    }

    /*--- In case this is a parallel simulation, we need to perform the
     Global2Local index transformation first. ---*/
    long *Global2Local;
    Global2Local = new long[geometry->GetGlobal_nPointDomain()];
    /*--- First, set all indices to a negative value by default ---*/
    for (iPoint = 0; iPoint < geometry->GetGlobal_nPointDomain(); iPoint++) {
      Global2Local[iPoint] = -1;
    }
    /*--- Now fill array with the transform values only for local points ---*/
    for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
      Global2Local[geometry->node[iPoint]->GetGlobalIndex()] = iPoint;
    }

    /*--- Read all lines in the restart file ---*/
    long iPoint_Local; unsigned long iPoint_Global = 0;\

    /* --- Skip coordinates ---*/
    unsigned short skipVars = nDim;

    /*--- Skip flow adjoint variables ---*/
    if (Kind_Solver == RUNTIME_TURB_SYS){
      skipVars += nDim+2;
    }

    /*--- The first line is the header ---*/
    getline (restart_file, text_line);

    while (getline (restart_file, text_line)) {
      istringstream point_line(text_line);

      /*--- Retrieve local index. If this node from the restart file lives
       on a different processor, the value of iPoint_Local will be -1.
       Otherwise, the local index for this node on the current processor
       will be returned and used to instantiate the vars. ---*/
      iPoint_Local = Global2Local[iPoint_Global];
      if (iPoint_Local >= 0) {
        point_line >> index;
        for (iVar = 0; iVar < skipVars; iVar++){ point_line >> dull_val;}
        for (iVar = 0; iVar < nVar; iVar++){ point_line >> Solution[iVar];}
        node[iPoint_Local] = new CDiscAdjVariable(Solution, nDim, nVar, config);
      }
      iPoint_Global++;
    }

    /*--- Instantiate the variable class with an arbitrary solution
     at any halo/periodic nodes. The initial solution can be arbitrary,
     because a send/recv is performed immediately in the solver. ---*/
    for (iPoint = nPointDomain; iPoint < nPoint; iPoint++) {
      node[iPoint] = new CDiscAdjVariable(Solution, nDim, nVar, config);
    }

    /*--- Close the restart file ---*/
    restart_file.close();

    /*--- Free memory needed for the transformation ---*/
    delete [] Global2Local;
  }
}

void CDiscAdjSolver::RegisterInput(CGeometry *geometry, CConfig *config){
  unsigned long iPoint, nPoint = geometry->GetnPoint();

  bool time_n_needed  = ((config->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
      (config->GetUnsteady_Simulation() == DT_STEPPING_2ND)),
  time_n1_needed = config->GetUnsteady_Simulation() == DT_STEPPING_2ND,
  input = true;

  /* --- Register solution at all necessary time instances and other variables on the tape ---*/

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    direct_solver->node[iPoint]->RegisterSolution(input);
  }
  if (time_n_needed){
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_time_n();
    }
  }
  if (time_n1_needed){
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_time_n1();
    }
  }
}

void CDiscAdjSolver::RegisterOutput(CGeometry *geometry, CConfig *config){

  unsigned long iPoint, nPoint = geometry->GetnPoint();

  /* --- Register variables as output of the solver iteration ---*/

  bool input = false;

  /* --- Register output variables on the tape --- */

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    direct_solver->node[iPoint]->RegisterSolution(input);
  }
}



void CDiscAdjSolver::RegisterObj_Func(CConfig *config){

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  /* --- Here we can add objective functions --- */

  switch (config->GetKind_ObjFunc()){
  case DRAG_COEFFICIENT:
      ObjFunc_Value = direct_solver->GetTotal_CDrag();
      break;
  case LIFT_COEFFICIENT:
      ObjFunc_Value = direct_solver->GetTotal_CLift();
      break;
  case SIDEFORCE_COEFFICIENT:
      ObjFunc_Value = direct_solver->GetTotal_CSideForce();
      break;
  case EFFICIENCY:
      ObjFunc_Value = direct_solver->GetTotal_CEff();
      break;
  case MOMENT_X_COEFFICIENT:
      ObjFunc_Value = direct_solver->GetTotal_CMx();
      break;
  case MOMENT_Y_COEFFICIENT:
      ObjFunc_Value = direct_solver->GetTotal_CMy();
      break;
  case MOMENT_Z_COEFFICIENT:
      ObjFunc_Value = direct_solver->GetTotal_CMz();
      break;
  case EQUIVALENT_AREA:
      ObjFunc_Value = direct_solver->GetTotal_CEquivArea();
      break;
  }
  ObjFunc_Value=config->GetScaleObj()*ObjFunc_Value;
  if (rank == MASTER_NODE){
    AD::RegisterOutput(ObjFunc_Value);
  }
}

void CDiscAdjSolver::RegisterConstraint_Func(CConfig *config){

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  if(config->GetPosConstraint()){
    ConstraintFunc_Value = config->GetTargetLift()-direct_solver->GetTotal_CLift();
  }else{
    ConstraintFunc_Value = -config->GetTargetLift()+direct_solver->GetTotal_CLift(); //ATTENTION was set before
  }
  ConstraintFunc_Value=config->GetScaleConstr()*ConstraintFunc_Value;
  if (rank == MASTER_NODE){
    AD::RegisterOutput(ConstraintFunc_Value);
  }
}

void CDiscAdjSolver::RegisterConstraint_Mom(CConfig *config){

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  if(config->GetPosConstraint()){
    ConstraintMom_Value =  -direct_solver->GetTotal_CMz();
  }else{
    ConstraintMom_Value = direct_solver->GetTotal_CMz();
  }
  ConstraintMom_Value=config->GetScaleConstr()*ConstraintMom_Value;
  if (rank == MASTER_NODE){
    AD::RegisterOutput(ConstraintMom_Value);
  }
}


void CDiscAdjSolver::SetAdj_ObjFunc(CGeometry *geometry, CConfig *config, double initVal){
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  if (rank == MASTER_NODE){
    SU2_TYPE::SetDerivative(ObjFunc_Value, initVal);
  } else {
    SU2_TYPE::SetDerivative(ObjFunc_Value, 0.0);
  }
}

void CDiscAdjSolver::SetAdj_ConstraintFunc(CGeometry *geometry, CConfig *config, double initVal){
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  if (rank == MASTER_NODE){
    SU2_TYPE::SetDerivative(ConstraintFunc_Value, initVal);
  } else {
    SU2_TYPE::SetDerivative(ConstraintFunc_Value, 0.0);
  }
}

su2double CDiscAdjSolver::GetConstraintFunc_Value(){
    return ConstraintFunc_Value;
}

void CDiscAdjSolver::StoreConstraint(){
    Constraint_Old=Constraint_Save;
    Constraint_Save= ConstraintFunc_Value;
}

double CDiscAdjSolver::GetMultiplier(){
    return multiplier;
}

void CDiscAdjSolver::SetMultiplier(CConfig *config, double value){
    multiplier=value;
    multiplierhelp=multiplier;
    multiplieroriginal=multiplier;

    cons_factor=SU2_TYPE::GetPrimary(config->GetConstraintFactor());
}

void CDiscAdjSolver::UpdateMultiplier(CConfig *config){
    //use this for +h and inequality
 /*   if(Constraint_Save>1E-3){ //neu: 6
            multiplier=multiplierhelp*(1+SU2_TYPE::GetPrimary(config->GetConstraintFactor())*SU2_TYPE::GetPrimary(Constraint_Save));
         //   multiplier=multiplierhelp-SU2_TYPE::GetPrimary(config->GetConstraintFactor())*SU2_TYPE::GetPrimary(Constraint_Save);
            multiplierhelp=multiplier;
    }else if(Constraint_Save<-1E-3 && config->GetEqualConstraint()){ //vorher 1E-3
            multiplier=-multiplierhelp*(1-SU2_TYPE::GetPrimary(config->GetConstraintFactor())*SU2_TYPE::GetPrimary(Constraint_Save)); //a
        //    multiplier=multiplierhelp-SU2_TYPE::GetPrimary(config->GetConstraintFactor())*SU2_TYPE::GetPrimary(Constraint_Save); //b
         //   multiplierhelp=multiplier; //a,b
            multiplierhelp=-multiplier; //c+a
    }else{
            multiplier=0;
            multiplierhelp=multiplieroriginal;
            std::cout<<"Multiplier set to 0"<<std::endl;
    }*/

    if(config->GetPosConstraint()){
        if(Constraint_Save<=0 && !(config->GetEqualConstraint())){
                multiplier=0;
                multiplierhelp=multiplieroriginal;
        }else{
            if(config->GetFactorIncrease()) cons_factor=cons_factor*(1+fabs(SU2_TYPE::GetPrimary(Constraint_Save))/fabs(SU2_TYPE::GetPrimary(Constraint_Save)-SU2_TYPE::GetPrimary(Constraint_Old)));
            if(config->GetPosUpdate()){
                multiplier=multiplierhelp+cons_factor*SU2_TYPE::GetPrimary(Constraint_Save);
            }else{
                multiplier=multiplierhelp-cons_factor*SU2_TYPE::GetPrimary(Constraint_Save);
            }
            multiplierhelp=multiplier;
        }
    }else{
        if(Constraint_Save>=0 && !(config->GetEqualConstraint())){
                multiplier=0;
                multiplierhelp=multiplieroriginal;
        }else{
            if(config->GetFactorIncrease()) cons_factor=cons_factor*(1+fabs(SU2_TYPE::GetPrimary(Constraint_Save))/fabs(SU2_TYPE::GetPrimary(Constraint_Save)-SU2_TYPE::GetPrimary(Constraint_Old)));
            if(config->GetPosUpdate()){
                multiplier=multiplierhelp+cons_factor*SU2_TYPE::GetPrimary(Constraint_Save);
            }else{
                multiplier=multiplierhelp-cons_factor*SU2_TYPE::GetPrimary(Constraint_Save);
            }
            multiplierhelp=multiplier;
        }
    }
    std::cout<<"Update of Multiplier: "<<multiplier<<std::endl;
    //cons_factor=cons_factor*1.1;
}

void CDiscAdjSolver::SetAdjointInput(CGeometry *geometry, CConfig *config){

  bool time_n_needed  = ((config->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
      (config->GetUnsteady_Simulation() == DT_STEPPING_2ND));

  bool time_n1_needed = config->GetUnsteady_Simulation() == DT_STEPPING_2ND;


  unsigned short iVar;
  unsigned long iPoint;
  su2double residual;

  /*--- Set Residuals to zero --- */

  for (iVar = 0; iVar < nVar; iVar++){
      SetRes_RMS(iVar,0.0);
      SetRes_Max(iVar,0.0,0);
  }

  for (iPoint = 0; iPoint < nPoint; iPoint++){

    /*--- Set the old solution --- */

    //node[iPoint]->Set_OldSolution();

    /*--- Extract the adjoint solution ---*/

    direct_solver->node[iPoint]->GetAdjointSolution(Solution);

    /*--- Store the adjoint solution ---*/

    node[iPoint]->SetSolution(Solution);
  }

  if (time_n_needed){
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Extract the adjoint solution at time n ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_time_n(Solution);

      /*--- Store the adjoint solution at time n ---*/

      node[iPoint]->Set_Solution_time_n(Solution);
    }
  }
  if (time_n1_needed){
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Extract the adjoint solution at time n-1 ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_time_n1(Solution);

      /*--- Store the adjoint solution at time n-1 ---*/

      node[iPoint]->Set_Solution_time_n1(Solution);
    }
  }

  /* --- Set the residuals --- */

  for (iPoint = 0; iPoint < nPointDomain; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
          residual = node[iPoint]->GetSolution(iVar) - node[iPoint]->GetSolution_Old(iVar);

          AddRes_RMS(iVar,residual*residual);
          AddRes_Max(iVar,fabs(residual),geometry->node[iPoint]->GetGlobalIndex(),geometry->node[iPoint]->GetCoord());
      }
  }

  SetResidual_RMS(geometry, config);
}

void CDiscAdjSolver::SetAdjointInputHelp(CGeometry *geometry, CConfig *config){ //do not overwrite oldSolution

  bool time_n_needed  = ((config->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
      (config->GetUnsteady_Simulation() == DT_STEPPING_2ND));

  bool time_n1_needed = config->GetUnsteady_Simulation() == DT_STEPPING_2ND;


  unsigned short iVar;
  unsigned long iPoint;
  su2double residual;

  /*--- Set Residuals to zero --- */

  for (iVar = 0; iVar < nVar; iVar++){
      SetRes_RMS(iVar,0.0);
      SetRes_Max(iVar,0.0,0);
  }

  for (iPoint = 0; iPoint < nPoint; iPoint++){

    /*--- Extract the adjoint solution ---*/

    direct_solver->node[iPoint]->GetAdjointSolution(Solution);

    /*--- Store the adjoint solution ---*/

    node[iPoint]->SetSolution(Solution);
  }

  if (time_n_needed){
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Extract the adjoint solution at time n ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_time_n(Solution);

      /*--- Store the adjoint solution at time n ---*/

      node[iPoint]->Set_Solution_time_n(Solution);
    }
  }
  if (time_n1_needed){
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Extract the adjoint solution at time n-1 ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_time_n1(Solution);

      /*--- Store the adjoint solution at time n-1 ---*/

      node[iPoint]->Set_Solution_time_n1(Solution);
    }
  }

  /* --- Set the residuals --- */

  for (iPoint = 0; iPoint < nPointDomain; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
          residual = node[iPoint]->GetSolution(iVar) - node[iPoint]->GetSolution_Old(iVar);

          AddRes_RMS(iVar,residual*residual);
          AddRes_Max(iVar,fabs(residual),geometry->node[iPoint]->GetGlobalIndex(),geometry->node[iPoint]->GetCoord());
      }
  }

  SetResidual_RMS(geometry, config);
}

void CDiscAdjSolver::StoreOldSolution(){
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->Set_OldSolution();
      node[iPoint]->Set_OldSolution();
    }
}

void CDiscAdjSolver::LoadOldSolution(){
    TotalIterations=TotalIterations+1;
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->SetSolution(direct_solver->node[iPoint]->GetSolution_Old());
      node[iPoint]->SetSolution(node[iPoint]->GetSolution_Old());
    }
}

void CDiscAdjSolver::StoreSolutionVec(unsigned short numQuad){
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->SetSolutionVec(numQuad);
      node[iPoint]->SetSolutionVec(numQuad);
    }
}

void CDiscAdjSolver::LoadSolutionVec(unsigned short numQuad){
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->SetSolution(direct_solver->node[iPoint]->GetSolutionVec(true,numQuad));
      node[iPoint]->SetSolution(node[iPoint]->GetSolutionVec(true,numQuad));
    }
}

void CDiscAdjSolver::StoreSolutionVecOld(unsigned short numQuad){
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->SetSolutionVecOld(numQuad);
      node[iPoint]->SetSolutionVecOld(numQuad);
    }
}

void CDiscAdjSolver::LoadSolutionVecOld(unsigned short numQuad){
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->SetSolution(direct_solver->node[iPoint]->GetSolutionVecOld(true,numQuad));
      node[iPoint]->SetSolution(node[iPoint]->GetSolutionVecOld(true,numQuad));
    }
}

void CDiscAdjSolver::LoadOldAdjoint(){
    unsigned long iPoint;
    unsigned short iVar;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
            node[iPoint]->SetSolution(node[iPoint]->GetSolution_Old());
    }
}

void CDiscAdjSolver::StoreSaveSolution(){
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      node[iPoint]->Set_SaveSolution();
      direct_solver->node[iPoint]->Set_SaveSolution();
    }
}

void CDiscAdjSolver::LoadSaveSolution(){
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
            node[iPoint]->SetSolution(node[iPoint]->GetSolution_Save());
            direct_solver->node[iPoint]->SetSolution(direct_solver->node[iPoint]->GetSolution_Save());
    }
}

void CDiscAdjSolver::OutputWritten(CGeometry *geometry){
    unsigned short iVar;
    unsigned long iPoint;
    unsigned long iVertex;
    for (unsigned short numQuad=0; numQuad<4; numQuad++){
        std::cout<<"SolutionVec_Old["<<numQuad<<"]=";
        for (iPoint = 0; iPoint < nPoint; iPoint++){
          for (iVar=0; iVar<nVar; iVar++){
              std::cout<<direct_solver->node[iPoint]->GetSolutionVec(true,numQuad)[iVar]<<" ";
          }
          //node[iPoint]->SetSolution(node[iPoint]->GetSolutionVec(true,numQuad));
        }
        std::cout<<std::endl;
    }
    /*---- DUMMY FUNCTION for writing arrays (call: solver_container[iZone][iMesh][ADJFLOW_SOL]->OutputWritten();) ----*/
    /*su2double norm=0;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] = 0.0;
        norm+=Solution[iVar]*Solution[iVar];
      }
      direct_solver->node[iPoint]->SetAdjointSolution(Solution);
    }
    std::cout<<"SetAdjointUpdate: "<<sqrt(norm)<<std::endl;*/
}

void CDiscAdjSolver::AssembleLagrangian(CConfig *config){
    unsigned short iVar;
    unsigned long iPoint;
    Lagrangian_Value=0.0;
    su2double helper=0.0;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        helper+=(direct_solver->node[iPoint]->GetSolution(iVar)-direct_solver->node[iPoint]->GetSolution_Old(iVar))*(direct_solver->node[iPoint]->GetSolution(iVar)-direct_solver->node[iPoint]->GetSolution_Old(iVar));
      }
    }
    if(config->GetOneShotConstraint()==true){
        helper+=ConstraintFunc_Value*ConstraintFunc_Value;
        helper=sqrt(helper/(nPoint*nVar+1))*(config->GetOneShotAlpha()/2);
    }else{
        helper=sqrt(helper/(nPoint*nVar))*(config->GetOneShotAlpha()/2);
    }
    Lagrangian_Value+=helper;
    helper=0.0;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        helper+=(node[iPoint]->GetSolution(iVar)-node[iPoint]->GetSolution_Old(iVar))*(node[iPoint]->GetSolution(iVar)-node[iPoint]->GetSolution_Old(iVar));
      }
    }
    Lagrangian_Value+=sqrt(helper/(nPoint*nVar))*(config->GetOneShotBeta()/2);
    Lagrangian_Value+=ObjFunc_Value;
    std::cout<<ObjFunc_Value<<" ";
    if(config->GetOneShotConstraint()==true){
        Lagrangian_Value+=ConstraintFunc_Value*multiplier;
    }
    helper=0.0;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        helper+=(direct_solver->node[iPoint]->GetSolution(iVar)-direct_solver->node[iPoint]->GetSolution_Old(iVar))*node[iPoint]->GetSolution_Old(iVar);
      }
    }
    Lagrangian_Value+=helper;
}

void CDiscAdjSolver::UpdateStateVariable(CConfig *config){
    unsigned long iPoint;
    unsigned short iVar;
    su2double stepsize=config->GetFDStep();
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] = direct_solver->node[iPoint]->GetSolution_Old(iVar)+stepsize*(node[iPoint]->GetSolution(iVar)-node[iPoint]->GetSolution_Old(iVar));
      }
      direct_solver->node[iPoint]->SetSolution(Solution);
    }
}

void CDiscAdjSolver::SetAdjointOutputUpdate(CGeometry *geometry, CConfig *config){
    unsigned short iVar;
    unsigned long iPoint;
    normy=0;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] = (direct_solver->node[iPoint]->GetSolution(iVar)-direct_solver->node[iPoint]->GetSolution_Old(iVar));
        normy+=Solution[iVar]*Solution[iVar];
      }
      direct_solver->node[iPoint]->SetAdjointSolution(Solution);
    }
    normy=sqrt(normy/(nPoint*nVar));
    if((normy/normyold) >0.01*rho){
        rho=(normy/normyold);
    }else{
        rho=0.01*rho;
    }
    normyold=normy;
}

void CDiscAdjSolver::SetAdjointOutputZero(CGeometry *geometry, CConfig *config){
    unsigned short iVar;
    unsigned long iPoint;
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] = 0.0;
      }
      direct_solver->node[iPoint]->SetAdjointSolution(Solution);
    }
}

void CDiscAdjSolver::SetAdjointOutput(CGeometry *geometry, CConfig *config){

  bool dual_time = (config->GetUnsteady_Simulation() == DT_STEPPING_1ST ||
      config->GetUnsteady_Simulation() == DT_STEPPING_2ND);

  unsigned short iVar;
  unsigned long iPoint;

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    for (iVar = 0; iVar < nVar; iVar++){
      Solution[iVar] = node[iPoint]->GetSolution(iVar);
    }
    if (dual_time){
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] += node[iPoint]->GetDual_Time_Derivative(iVar);
      }
    }
    direct_solver->node[iPoint]->SetAdjointSolution(Solution);
  }
}

void CDiscAdjSolver::ResetSensitivity(CGeometry *geometry){
    unsigned short iMarker=0;
    unsigned long iVertex;

    //for (iMarker = 0; iMarker < nMarker; iMarker++){
        for (iVertex = 0; iVertex < geometry->GetnVertex(iMarker); iVertex++){
          LagrangeSens[iMarker][iVertex] = 0.0;
        }
    //}
}

void CDiscAdjSolver::UpdateLagrangeSensitivity(CGeometry *geometry, su2double factor){
    unsigned short iMarker=0;
    unsigned long iVertex;
    /*if(factor==200)     factor=2/((1-rho)*(1-rho));*/
    cout.precision(15);
    std::cout<<"factor: "<<factor<<std::endl;
   // for (iMarker = 0; iMarker < nMarker; iMarker++){
        for (iVertex = 0; iVertex < geometry->GetnVertex(iMarker); iVertex++){
          LagrangeSens[iMarker][iVertex] += factor*CSensitivity[iMarker][iVertex];
          std::cout<<factor*CSensitivity[iMarker][iVertex]<<" ";
        }
   // }
    std::cout<<std::endl;
}

void CDiscAdjSolver::OverwriteSensitivityProjected(CGeometry *geometry){
    unsigned long iVertex;
    for (iVertex = 0; iVertex < geometry->GetnVertex(0); iVertex++){
          geometry->vertex[0][iVertex]->SetAuxVar(LagrangeSens[0][iVertex]);
    }
    std::cout<<std::endl;
}

void CDiscAdjSolver::OverwriteGradientProjected(CGeometry *geometry){
    unsigned long iVertex;
    for (iVertex = 0; iVertex < geometry->GetnVertex(0); iVertex++){
          geometry->vertex[0][iVertex]->SetAuxVar(CSensitivityOld[0][iVertex]);
    }
    std::cout<<std::endl;
}

void CDiscAdjSolver::SetProjectedSensitivity(unsigned long iDV, su2double value){
    ProjectedSens[iDV]=value;
}

void CDiscAdjSolver::SetProjectedGradient(unsigned long iDV, su2double value){
    ProjectedGradient[iDV]=value;
}

void CDiscAdjSolver::ApplyDesignVar(){
    unsigned long iVertex;
    for (iVertex = 0; iVertex < 38; iVertex++){
        DesignVar[iVertex]+=DesignVarUpdateReal[iVertex];
    }
}

su2double CDiscAdjSolver::getDVValue(unsigned long iDV){
    return DesignVarUpdate[iDV];
}

void CDiscAdjSolver::SaveSurfaceSensitivity(CGeometry *geometry){
    unsigned short iMarker=0;
    unsigned long iVertex;

 //   for (iMarker = 0; iMarker < nMarker; iMarker++){
        for (iVertex = 0; iVertex < geometry->GetnVertex(iMarker); iVertex++){
          CSensitivityOld[iMarker][iVertex] = CSensitivity[iMarker][iVertex];
        }
 //   }
}

void CDiscAdjSolver::ResetExpValues(CGeometry *geometry){
    unsigned long iVertex;
        for (iVertex = 0; iVertex < geometry->GetnVertex(0); iVertex++){
          ExpCSensitivityOld[0][iVertex] = 0.0;
          ExpLagrangeSens[0][iVertex]=0.0;
        }
        ExpConstraint_Save=0.0;
        ExpObjFunc_Value=0.0;
        ExpLagrangian_Value=0.0;
}

void CDiscAdjSolver::SumExpValues(CGeometry *geometry,unsigned short numQuad){
    unsigned long iVertex;
        for (iVertex = 0; iVertex < geometry->GetnVertex(0); iVertex++){
          ExpCSensitivityOld[0][iVertex] += weights[numQuad]*CSensitivityOld[0][iVertex];
          ExpLagrangeSens[0][iVertex] += weights[numQuad]*LagrangeSens[0][iVertex];
        }
        ExpConstraint_Save+=weights[numQuad]*Constraint_Save;
        ExpObjFunc_Value+=weights[numQuad]*ObjFunc_Value;
        ExpLagrangian_Value+=weights[numQuad]*Lagrangian_Value;
}

su2double CDiscAdjSolver::GetMachP(unsigned short numQuad){
    return machp[numQuad];
}

void CDiscAdjSolver::DistributeExpValues(CGeometry *geometry){
    unsigned long iVertex;
    su2double pi = 3.141592653589793;
        for (iVertex = 0; iVertex < geometry->GetnVertex(0); iVertex++){
          CSensitivityOld[0][iVertex] = 1./sqrt(pi)*ExpCSensitivityOld[0][iVertex];
          LagrangeSens[0][iVertex]=1./sqrt(pi)*ExpLagrangeSens[0][iVertex];
        }
        ObjFunc_Value=1./sqrt(pi)*ExpObjFunc_Value;
        Constraint_Save=1./sqrt(pi)*ExpConstraint_Save;
        Lagrangian_Value=1./sqrt(pi)*ExpLagrangian_Value;
}

su2double CDiscAdjSolver::SensitivityNorm(CGeometry *geometry){
    unsigned long iVertex;
    su2double norm=0;
        for (iVertex = 0; iVertex < geometry->GetnVertex(0); iVertex++){
             norm+=CSensitivity[0][iVertex]*CSensitivity[0][iVertex];
        }
    norm=sqrt(norm);
    return norm;
}

void CDiscAdjSolver::SetSensitivity(CGeometry *geometry, CConfig *config){

  unsigned long iPoint;
  unsigned short iDim;
  su2double *Coord, Sensitivity;

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    Coord = geometry->node[iPoint]->GetCoord();

    for (iDim = 0; iDim < nDim; iDim++){
      Sensitivity = SU2_TYPE::GetDerivative(Coord[iDim]);

      node[iPoint]->SetSensitivity(iDim, Sensitivity);
    }
  }
  SetSurface_Sensitivity(geometry, config);
}

void CDiscAdjSolver::WriteDesignVariable(){
    ofstream myfile;
    unsigned long iVertex;

    myfile.open ("designvar.txt");
    for (iVertex=0;iVertex<38;iVertex++)
    {
        myfile <<std::setprecision(16)<<DesignVarUpdate[iVertex]<<std::endl;

    }
    myfile.close();
}

void CDiscAdjSolver::DesignUpdate(CGeometry *geometry, CConfig *config){
    unsigned long iVertex,jVertex;
    for (iVertex = 0; iVertex < geometry->GetnVertex(0); iVertex++){
        UpdateSens[iVertex]=0.0;
        for (jVertex = 0; jVertex < geometry->GetnVertex(0); jVertex++){
            UpdateSens[iVertex]-=Hess[iVertex][jVertex]*CSensitivityOld[0][jVertex];
        }

    }
}

void CDiscAdjSolver::DesignStep(su2double values){
    unsigned long iVertex;
    for (iVertex = 0; iVertex < 38; iVertex++){
        DesignVarUpdate[iVertex]=values;
    }
}

void CDiscAdjSolver::DesignMinus(){
    unsigned long iVertex;
    for (iVertex = 0; iVertex < 38; iVertex++){
        DesignVarUpdate[iVertex]=DesignVarUpdateReal[iVertex]-DesignVarUpdateSave[iVertex];
    }
}

void CDiscAdjSolver::CalculatePhi(su2double steplen, su2double& Phi, su2double& dPhi){
    su2double helper=0.0;
    if(steplen==0.0){
        unsigned long iVertex;
        for (iVertex = 0; iVertex < 38; iVertex++){
            helper+=UpdateSens[iVertex]*ProjectedSensOld[iVertex];
        }
        dPhi=helper;
        Phi=Lagrangian_Value_Old;
    }else{
        unsigned long iVertex;
        for (iVertex = 0; iVertex < 38; iVertex++){
            helper+=UpdateSens[iVertex]*ProjectedSens[iVertex];
        }
        dPhi=helper;
        Phi=Lagrangian_Value;
    }
}

su2double CDiscAdjSolver::QuadraticApproximation(su2double steplen){
    su2double helper=0.0;
    unsigned long iVertex;
    for (iVertex = 0; iVertex < 38; iVertex++){
        helper+=UpdateSens[iVertex]*ProjectedSens[iVertex];
    }
    GradPhiCubic=helper;
    PhiCubic=Lagrangian_Value_Old;
    PhiOld=Lagrangian_Value;
    StepOld=steplen;
    return ((-helper*steplen*steplen)/(2*(Lagrangian_Value-Lagrangian_Value_Old-helper*steplen)));
}



bool CDiscAdjSolver::CheckDescentDirection(su2double steplen){
    su2double helper=0.0; //Phi'(0)
    unsigned long iVertex;
    for (iVertex = 0; iVertex < 38; iVertex++){
        helper+=UpdateSens[iVertex]*ProjectedSens[iVertex];
    }
    if(helper<=0) return true;
    else{
        /*for (iVertex = 0; iVertex < 38; iVertex++){
            UpdateSens[iVertex]=-UpdateSens[iVertex];
        }*/
        return false;
    }
}

void CDiscAdjSolver::ChangeDirection(){
    unsigned long iVertex;
        for (iVertex = 0; iVertex < 38; iVertex++){
            UpdateSens[iVertex]=-UpdateSens[iVertex];
        }
}

su2double CDiscAdjSolver::CubicApproximation(su2double steplen){
    su2double avalue=StepOld*StepOld*(Lagrangian_Value-PhiCubic-GradPhiCubic*steplen)-steplen*steplen*(PhiOld-PhiCubic-GradPhiCubic*StepOld);
    su2double bvalue=-StepOld*StepOld*StepOld*(Lagrangian_Value-PhiCubic-GradPhiCubic*steplen)+steplen*steplen*steplen*(PhiOld-PhiCubic-GradPhiCubic*StepOld);
    avalue=1.0/(StepOld*StepOld*steplen*steplen*(steplen-StepOld))*avalue;
    bvalue=1.0/(StepOld*StepOld*steplen*steplen*(steplen-StepOld))*bvalue;
    su2double steplennew;
    steplennew=(-bvalue+sqrt(bvalue*bvalue-3*avalue*GradPhiCubic))/(3*avalue);
    PhiOld=Lagrangian_Value;
    StepOld=steplen;
    if(fabs(steplennew-steplen)<1E-15){
        std::cout<<"small correction: "<<steplennew<<std::endl;
        steplen=steplen*0.5;
    }else if(fabs(steplennew-steplen)>0.5 || steplennew==0){
        std::cout<<"big correction: "<<steplennew<<std::endl;
        steplen=steplen*0.5;
    }else{
        std::cout<<"CUBIC: "<<steplennew<<std::endl;
         steplen = steplennew;
    }
    return steplen;
}

void CDiscAdjSolver::DesignUpdateProjected(CGeometry *geometry, CConfig *config, unsigned short ExtIter, su2double steplen){
    unsigned long iVertex;
    su2double normsens=0;
    for (iVertex = 0; iVertex < 38; iVertex++){
        DesignVarUpdateSave[iVertex]=DesignVarUpdateReal[iVertex];
        DesignVarUpdate[iVertex]=0.0;
        normsens+=UpdateSens[iVertex]*UpdateSens[iVertex];
    }
    normsens=sqrt(normsens/(38*38));
    std::cout<<"Norm of Update: "<<normsens<<std::endl;

    for (iVertex = 0; iVertex < 38; iVertex++){
        DesignVarUpdate[iVertex]=UpdateSens[iVertex]*steplen;
        if(config->GetOSStepAdaptive()){
            if (normsens>10000000){ //vorher normsens>100000
                        DesignVarUpdate[iVertex]=DesignVarUpdate[iVertex]*1E-3; //was >100, 1E-2
            }
        /*    else if(normsens>1&&config->GetExtIter()>1416){ //vorher normsens>100000, 1E-1
                        DesignVarUpdate[iVertex]=DesignVarUpdate[iVertex]*1E-3; //was >1, 1E-1
            }*/
     /*       else if(normsens<=100000){
                        DesignVarUpdate[iVertex]=DesignVarUpdate[iVertex]*1.0; // was <1
            }*/
        }
        //set box constraints
        if((DesignVar[iVertex]+DesignVarUpdate[iVertex])>0.005)  DesignVarUpdate[iVertex]=0.005-DesignVar[iVertex];
        if((DesignVar[iVertex]+DesignVarUpdate[iVertex])<-0.005) DesignVarUpdate[iVertex]=-0.005-DesignVar[iVertex];

        DesignVarUpdateReal[iVertex]=DesignVarUpdate[iVertex];
    }
    std::cout<<std::endl;
}

su2double CDiscAdjSolver::DesignUpdateBounds(CGeometry *geometry, CConfig *config, unsigned short ExtIter, su2double steplen){
    unsigned long iVertex;
    su2double normsens=0;
    for (iVertex = 0; iVertex < 38; iVertex++){
        DesignVarUpdateSave[iVertex]=DesignVarUpdateReal[iVertex];
        DesignVarUpdate[iVertex]=0.0;
        normsens+=UpdateSens[iVertex]*UpdateSens[iVertex];
    }
    normsens=sqrt(normsens/(38*38));
    std::cout<<"Norm of Update: "<<normsens<<std::endl;

    for (iVertex = 0; iVertex < 38; iVertex++){
        while((DesignVar[iVertex]+UpdateSens[iVertex]*steplen)>0.005||(DesignVar[iVertex]+UpdateSens[iVertex]*steplen)<-0.005){
            steplen=steplen*0.5;
        }
    }
    for (iVertex = 0; iVertex < 38; iVertex++){
        DesignVarUpdate[iVertex]=UpdateSens[iVertex]*steplen;
        DesignVarUpdateReal[iVertex]=DesignVarUpdate[iVertex];
    }
    if(steplen<1E-30) std::cout<<"REACHED DESIGN VARIABLE BOUNDS"<<std::endl;
    return steplen;
}

bool CDiscAdjSolver::CheckFirstWolfe(su2double steplen){
    su2double helper=0.0;
    unsigned long iVertex;
    std::cout<<"LagrangeOld: "<<Lagrangian_Value_Old<<", LagrangeNew: "<<Lagrangian_Value<<", Stepsize: "<<steplen<<std::endl;
    for (iVertex = 0; iVertex < 38; iVertex++){
        helper+=DesignVarUpdateReal[iVertex]*ProjectedSens[iVertex];//UpdateSens[iVertex]*ProjectedSens[iVertex];
    }
    if (Lagrangian_Value<=(Lagrangian_Value_Old+1E-4*helper)){ //*steplen*helper)){
        return false;
    }
    else{
        std::cout<<"First Wolfe Condition not satisfied!"<<std::endl;
         return true;
       // return false;
    }
}

void CDiscAdjSolver::BFGSUpdateProjected(CGeometry *geometry, CConfig *config, unsigned short ExtIter){

    unsigned long iVertex, jVertex, kVertex, lVertex, mVertex;
    su2double *rk,*duk,*wone;
    rk=new su2double[38];
    duk=new su2double[38];
    wone=new su2double[38];
    su2double vk=0;
    su2double normrk=0;
    su2double normduk=0;

    //Output of Gradients and Information

    std::cout<<"Projected Gradient of Augmented Lagrangian "<<std::endl;
    for (iVertex=0;iVertex<38;iVertex++)
    {
        std::cout<<ProjectedSens[iVertex]<<" ";
    }
    std::cout<<std::endl;
    std::cout<<"iterationcount: "<<TotalIterations<<std::endl;
    std::cout<<"objfuncvalue: "<<ObjFunc_Value<<std::endl;
    std::cout<<"constraintvalue: "<<Constraint_Save<<std::endl;
    std::cout<<"Projected Gradient N_u"<<std::endl;
    for (iVertex=0;iVertex<38;iVertex++)
    {
        std::cout<<ProjectedGradient[iVertex]<<" ";
    }
    std::cout<<std::endl;


    if(ExtIter>config->GetOneShotStart()){
        for (iVertex = 0; iVertex < 38; iVertex++){
            //DesignVar[iVertex]+=DesignVarUpdate[iVertex];
            rk[iVertex]=ProjectedSens[iVertex]-ProjectedSensOld[iVertex];
            duk[iVertex]=DesignVarUpdate[iVertex];
            vk+=rk[iVertex]*duk[iVertex];
            normrk+=rk[iVertex]*rk[iVertex];
            normduk+=duk[iVertex]*duk[iVertex];
        }
        std::cout<<std::endl;
        std::cout<<"vk "<<vk<<std::endl;
        std::cout<<"normduk "<<normduk<<", normrk "<<normrk<<", vk/normduk "<<vk/normduk<<std::endl;
        if(config->GetDampedBFGS()){
            //normalization
            for (iVertex=0;iVertex<38;iVertex++){
                rk[iVertex]=rk[iVertex]/sqrt(normrk);
                duk[iVertex]=duk[iVertex]/sqrt(normduk);
                if(config->NormalizeHB()){
                for (jVertex=0;jVertex<38;jVertex++){
                    Bess[iVertex][jVertex]=Bess[iVertex][jVertex]*sqrt(normduk)/sqrt(normrk);
                    Hess[iVertex][jVertex]=Hess[iVertex][jVertex]*sqrt(normrk)/sqrt(normduk);
                }
                }
            }
            //we store the Hessian approximation (Bess) and the inverse (Hess)
            su2double sBs=0.0;
            vk=0;
            su2double bmin=config->GetDampedMin();
            su2double bmax=config->GetDampedMax();
            su2double theta=1.0;
            su2double** MatA;
            MatA=new su2double*[38];
            for (iVertex=0;iVertex<38;iVertex++){
                MatA[iVertex]=new su2double[38];
                for (jVertex=0;jVertex<38;jVertex++){
                    MatA[iVertex][jVertex]=0.0;
                }
            }
            for (iVertex=0;iVertex<38;iVertex++){
                vk+=rk[iVertex]*duk[iVertex];
                for (jVertex=0;jVertex<38;jVertex++){
                    sBs+=duk[iVertex]*Bess[iVertex][jVertex]*duk[jVertex];
                }
            }
            if(config->GetDampedBFGSPow()){
               su2double gamma=config->GetDampedGamma();
               if(vk<gamma*sBs){
                   theta=0.9*((1-gamma)*sBs)/(sBs-vk);
               }
               std::cout<<"correction: "<<theta<<std::endl;
            }else{
            if(((vk/normduk)<bmin)||((vk/normduk)>bmax)){
                su2double useb;
                if((vk/normduk)<bmin){
                    useb=bmin;
                }else{
                    useb=bmax;
                }
                for (iVertex=0;iVertex<38;iVertex++){
                    for (jVertex=0;jVertex<38;jVertex++){
                        if(iVertex==jVertex){
                            theta+=duk[iVertex]*(Bess[iVertex][jVertex]-useb)*duk[jVertex];
                        }else{
                            theta+=duk[iVertex]*(Bess[iVertex][jVertex])*duk[jVertex];
                        }
                    }
                }
                theta=0.9*(theta/(sBs-vk));
                std::cout<<"correction: "<<theta<<std::endl;
            }
            }
            su2double normnewrk=0;
            for (iVertex=0;iVertex<38;iVertex++){
                rk[iVertex]=theta*rk[iVertex];
                for (jVertex=0;jVertex<38;jVertex++){
                    rk[iVertex]+=(1-theta)*Bess[iVertex][jVertex]*duk[jVertex];
                }
                normnewrk+=rk[iVertex]*rk[iVertex];
            }
            normnewrk=sqrt(normnewrk);
            if(config->NormalizeNewY()){
            for (iVertex=0;iVertex<38;iVertex++){
                rk[iVertex]=rk[iVertex]/normnewrk;
            }
            }
            vk=0;
            for (iVertex=0;iVertex<38;iVertex++){
                vk+=rk[iVertex]*duk[iVertex];
            }
            std::cout<<"vknew "<<vk<<std::endl;
            //normal Update
            //for Bess
            su2double* Bs=new su2double[38];
            for (iVertex=0;iVertex<38;iVertex++){
                Bs[iVertex]=0;
                for (jVertex=0;jVertex<38;jVertex++){
                    Bs[iVertex]+=Bess[iVertex][jVertex]*duk[jVertex];
                }
            }
            for (iVertex=0;iVertex<38;iVertex++){
                for (jVertex=0;jVertex<38;jVertex++){
                    MatA[iVertex][jVertex]=Bess[iVertex][jVertex]+(1.0/vk)*rk[iVertex]*rk[jVertex]-(1.0/sBs)*Bs[iVertex]*Bs[jVertex];
                        /*for (lVertex=0; lVertex<38; lVertex++){
                            for (mVertex=0; mVertex<38; mVertex++){
                                MatA[iVertex][jVertex]-=(1.0/sBs)*Bess[iVertex][lVertex]*duk[lVertex]*duk[mVertex]*Bess[mVertex][jVertex];
                            }
                        }*/
                }
            }
            delete [] Bs;
            for (iVertex=0;iVertex<38;iVertex++){
                for (jVertex=0;jVertex<38;jVertex++){
                    Bess[iVertex][jVertex]=MatA[iVertex][jVertex];
                }
            }
            //for Hess
            for (iVertex=0;iVertex<38;iVertex++){
                for (jVertex=0;jVertex<38;jVertex++){
                    MatA[iVertex][jVertex]=Hess[iVertex][jVertex]+(1.0/vk)*duk[iVertex]*duk[jVertex];
                    for (kVertex=0; kVertex<38; kVertex++){
                        MatA[iVertex][jVertex]+=-(1.0/vk)*duk[iVertex]*Hess[jVertex][kVertex]*rk[kVertex]-(1.0/vk)*duk[jVertex]*Hess[iVertex][kVertex]*rk[kVertex];
                        for (lVertex=0; lVertex<38; lVertex++){
                            MatA[iVertex][jVertex]+=(1.0/vk)*(1.0/vk)*duk[iVertex]*duk[jVertex]*rk[lVertex]*Hess[lVertex][kVertex]*rk[kVertex];
                        }
                    }
                }
            }
            for (iVertex=0;iVertex<38;iVertex++){
                for (jVertex=0;jVertex<38;jVertex++){
                    Hess[iVertex][jVertex]=MatA[iVertex][jVertex];
                }
            }
            for (iVertex=0;iVertex<38;iVertex++){
                rk[iVertex]=rk[iVertex]*sqrt(normrk);
                duk[iVertex]=duk[iVertex]*sqrt(normduk);
                if(config->NormalizeHB()){
                for (jVertex=0;jVertex<38;jVertex++){
                    Bess[iVertex][jVertex]=Bess[iVertex][jVertex]*sqrt(normrk)/sqrt(normduk);
                    Hess[iVertex][jVertex]=Hess[iVertex][jVertex]*sqrt(normduk)/sqrt(normrk);
                }
                }
            }
        }else{

        if (vk>0 && ((fabs(vk)>1E-3) || (config->GetCheckVk()==false))){
            //HESSOLD
     /*   for (iVertex = 0; iVertex < 38; iVertex++){
          wone[iVertex]=0.0;
          for (jVertex = 0; jVertex < 38; jVertex++){
            wone[iVertex]+=Hess[iVertex][jVertex]*rk[jVertex];
          }
        }
        for (iVertex = 0; iVertex < 38; iVertex++){
          wtwo+=rk[iVertex]*wone[iVertex];
        }
        for (iVertex = 0; iVertex < 38; iVertex++){
          for (jVertex = 0; jVertex < 38; jVertex++){
            Hess[iVertex][jVertex]=Hess[iVertex][jVertex]-(1.0/vk)*(wone[iVertex]*duk[jVertex]+wone[jVertex]*duk[iVertex])+(1.0/vk)*(1+wtwo/vk)*duk[iVertex]*duk[jVertex];
          }
        }*/
        su2double** MatA;//, **MatB, **MatC;
        MatA=new su2double*[38];
     //   MatB=new su2double*[38];
      //  MatC=new su2double*[38];
        for (iVertex=0;iVertex<38;iVertex++){
            MatA[iVertex]=new su2double[38];
        //    MatB[iVertex]=new su2double[38];
        //    MatC[iVertex]=new su2double[38];
            for (jVertex=0;jVertex<38;jVertex++){
                MatA[iVertex][jVertex]=0.0;
       //         MatB[iVertex][jVertex]=0.0;
       //         MatC[iVertex][jVertex]=0.0;
            }
        }
      /*  for (iVertex=0;iVertex<38;iVertex++){
            for (jVertex=0;jVertex<38;jVertex++){
                MatA[iVertex][jVertex]=duk[iVertex]*rk[jVertex];
            }
        }
        for (iVertex=0;iVertex<38;iVertex++){
            for (jVertex=0;jVertex<38;jVertex++){
                for (kVertex=0; kVertex<38; kVertex++){
                    if(iVertex==kVertex) MatB[iVertex][jVertex]+=(1.0-(1.0/vk)*MatA[iVertex][kVertex])*Hess[kVertex][jVertex];
                    else MatB[iVertex][jVertex]+=(-(1.0/vk)*MatA[iVertex][kVertex])*Hess[kVertex][jVertex];
                }
            }
        }
        for (iVertex=0;iVertex<38;iVertex++){
            for (jVertex=0;jVertex<38;jVertex++){
                for (kVertex=0; kVertex<38; kVertex++){
                    if(jVertex==kVertex) MatC[iVertex][jVertex]+=MatB[iVertex][kVertex]*(1.0-(1.0/vk)*MatA[jVertex][kVertex]);
                    else  MatC[iVertex][jVertex]+=MatB[iVertex][kVertex]*(-(1.0/vk)*MatA[jVertex][kVertex]);
                }
            }
        }
        for (iVertex=0;iVertex<38;iVertex++){
            for (jVertex=0;jVertex<38;jVertex++){
                Hess[iVertex][jVertex]=MatC[iVertex][jVertex]+(1.0/vk)*duk[iVertex]*duk[jVertex];
            }
        }*/
        //HESS2
        if(config->GetLBFGS()){
            unsigned long maxCount=config->GetLBFGSNum()-1;
            if(maxCount>BFGSCount) maxCount=BFGSCount;
            //std::cout<<maxCount<<std::endl;
            for(unsigned long kCount=0;kCount<maxCount;kCount++){
                for (iVertex=0;iVertex<38;iVertex++){
                    rkStore[kCount+1][iVertex]=rkStore[kCount][iVertex];
                    dukStore[kCount+1][iVertex]=dukStore[kCount][iVertex];
                }
            }
            if(config->GetHInit()) std::cout<<"Initialize H with "<<vk/normrk<<std::endl;
            for (iVertex=0;iVertex<38;iVertex++){
                rkStore[maxCount][iVertex]=rk[iVertex];
                dukStore[maxCount][iVertex]=duk[iVertex];
                for (jVertex=0;jVertex<38;jVertex++){
                    Hess[iVertex][jVertex]=0;
                    if(iVertex==jVertex) Hess[iVertex][jVertex]=1.0;
                    if(config->GetHInit()&& (iVertex==jVertex)){
                        Hess[iVertex][jVertex]=vk/normrk;
                    }
                }
            }
            for (unsigned long kCount=0;kCount<=maxCount;kCount++){
                vk=0;
                for (iVertex=0;iVertex<38;iVertex++){
                    vk+=rkStore[kCount][iVertex]*dukStore[kCount][iVertex];
                }
                for (iVertex=0;iVertex<38;iVertex++){
                    for (jVertex=0;jVertex<38;jVertex++){
                        MatA[iVertex][jVertex]=Hess[iVertex][jVertex]+(1.0/vk)*dukStore[kCount][iVertex]*dukStore[kCount][jVertex];
                        for (kVertex=0; kVertex<38; kVertex++){
                            MatA[iVertex][jVertex]+=-(1.0/vk)*dukStore[kCount][iVertex]*Hess[jVertex][kVertex]*rkStore[kCount][kVertex]-(1.0/vk)*dukStore[kCount][jVertex]*Hess[iVertex][kVertex]*rkStore[kCount][kVertex];
                            for (lVertex=0; lVertex<38; lVertex++){
                                MatA[iVertex][jVertex]+=(1.0/vk)*(1.0/vk)*dukStore[kCount][iVertex]*dukStore[kCount][jVertex]*rkStore[kCount][lVertex]*Hess[lVertex][kVertex]*rkStore[kCount][kVertex];
                            }
                        }
                    }
                }
                for (iVertex=0;iVertex<38;iVertex++){
                    for (jVertex=0;jVertex<38;jVertex++){
                        Hess[iVertex][jVertex]=MatA[iVertex][jVertex];
                    }
                }
            }
        }else{
            for (iVertex=0;iVertex<38;iVertex++){
                for (jVertex=0;jVertex<38;jVertex++){
                    MatA[iVertex][jVertex]=Hess[iVertex][jVertex]+(1.0/vk)*duk[iVertex]*duk[jVertex];
                    for (kVertex=0; kVertex<38; kVertex++){
                        MatA[iVertex][jVertex]+=-(1.0/vk)*duk[iVertex]*Hess[jVertex][kVertex]*rk[kVertex]-(1.0/vk)*duk[jVertex]*Hess[iVertex][kVertex]*rk[kVertex];
                        for (lVertex=0; lVertex<38; lVertex++){
                            MatA[iVertex][jVertex]+=(1.0/vk)*(1.0/vk)*duk[iVertex]*duk[jVertex]*rk[lVertex]*Hess[lVertex][kVertex]*rk[kVertex];
                        }
                    }
                }
            }
            for (iVertex=0;iVertex<38;iVertex++){
                for (jVertex=0;jVertex<38;jVertex++){
                    Hess[iVertex][jVertex]=MatA[iVertex][jVertex];
                }
            }
        }

        for (iVertex=0;iVertex<38;iVertex++){
            delete [] MatA[iVertex];
        //     delete [] MatB[iVertex];
        //     delete [] MatC[iVertex];
        }
        delete [] MatA;
       // delete [] MatB;
       // delete [] MatC;
        BFGSCount=BFGSCount+1;
        }else{
            std::cout<<"!!!!!!!!!!!!!!!!ATTENTION-HESSIAN NON-POSITIVE-DEFINITE!!!!!!!!!!!!!!!!!!!"<<std::endl;
            if(config->GetIdentityHessian()){
            for (iVertex = 0; iVertex < 38; iVertex++){
              for (jVertex = 0; jVertex < 38; jVertex++){
                Hess[iVertex][jVertex]=0.0;
                if(iVertex==jVertex) Hess[iVertex][jVertex]=1.0;
                if(config->GetHInit()&& (iVertex==jVertex)) Hess[iVertex][jVertex]=config->GetHScale();
              }
            }
            }
        }
        }
    }

    std::cout<<"Design Variable "<<std::endl;
    for (iVertex=0;iVertex<38;iVertex++)
    {
        std::cout<<DesignVar[iVertex]<<" ";
    }
    std::cout<<std::endl;

        for (iVertex = 0; iVertex < 38; iVertex++){
          ProjectedSensOld[iVertex] = ProjectedSens[iVertex];
       //   DesignVarOld[iVertex]=DesignVar[iVertex];
        }

        Lagrangian_Value_Old=Lagrangian_Value;

        for (iVertex = 0; iVertex < 38; iVertex++){

            UpdateSens[iVertex]=0.0;
            for (jVertex = 0; jVertex < 38; jVertex++){
                UpdateSens[iVertex]-=Hess[iVertex][jVertex]*ProjectedGradient[jVertex];

            }
        }
     delete [] rk;
     delete [] duk;
     delete [] wone;

}

void CDiscAdjSolver::SetSensitivityFD(CGeometry *geometry, CConfig *config){

  unsigned long iPoint;
  unsigned short iDim;
  su2double *Coord, Sensitivity;
  unsigned short iMarker=0;
  unsigned long iVertex;
  su2double *Normal, Area, Prod, Sens = 0.0, SensDim;
  su2double Total_Sens_Geo_local = 0.0;
  Total_Sens_Geo = 0.0;
  su2double stepsize=config->GetFDStep();

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    Coord = geometry->node[iPoint]->GetCoord();

    for (iDim = 0; iDim < nDim; iDim++){
      Sensitivity = SU2_TYPE::GetDerivative(Coord[iDim]);

      node[iPoint]->SetSensitivity(iDim, Sensitivity);
    }
  }
  std::cout<<"FDStep: "<<stepsize<<std::endl;
//  for (iMarker = 0; iMarker < nMarker; iMarker++){
    Sens_Geo[iMarker] = 0.0;
    /* --- Loop over boundary markers to select those for Euler walls and NS walls --- */

    if(config->GetMarker_All_KindBC(iMarker) == EULER_WALL
       || config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX
       || config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL){
      for (iVertex = 0; iVertex < geometry->GetnVertex(iMarker); iVertex++){
        iPoint = geometry->vertex[iMarker][iVertex]->GetNode();
        Normal = geometry->vertex[iMarker][iVertex]->GetNormal();
        Prod = 0.0;
        Area = 0.0;
        for (iDim = 0; iDim < nDim; iDim++){
          /* --- retrieve the gradient calculated with AD -- */
          SensDim = node[iPoint]->GetSensitivity(iDim);

          /* --- calculate scalar product for projection onto the normal vector ---*/
          Prod += Normal[iDim]*SensDim;
          Area += Normal[iDim]*Normal[iDim];
        }
        Area = sqrt(Area);

        /* --- projection of the gradient
         *     calculated with AD onto the normal
         *     vector of the surface --- */
        //Sens = Prod;
        Sens=Prod/Area;

        /*--- Compute sensitivity for each surface point ---*/
        CSensitivity[iMarker][iVertex] = -Sens;
        if (geometry->node[iPoint]->GetFlip_Orientation())
          CSensitivity[iMarker][iVertex] = -CSensitivity[iMarker][iVertex];
        CSensitivity[iMarker][iVertex] = (CSensitivity[iMarker][iVertex]-CSensitivityOld[iMarker][iVertex])/stepsize;

        if (geometry->node[iPoint]->GetDomain()){
          Sens_Geo[iMarker] += Sens*Sens;
        }
      }
      Total_Sens_Geo_local += sqrt(Sens_Geo[iMarker]);

    }
 // }

#ifdef HAVE_MPI
  SU2_MPI::Allreduce(&Total_Sens_Geo_local,&Total_Sens_Geo,1,MPI_DOUBLE,MPI_SUM, MPI_COMM_WORLD);
#else
  Total_Sens_Geo = Total_Sens_Geo_local;
#endif
}

void CDiscAdjSolver::SetSurface_Sensitivity(CGeometry *geometry, CConfig *config){
  unsigned short iMarker=0,iDim;
  unsigned long iVertex, iPoint;
  su2double *Normal, Area, Prod, Sens = 0.0, SensDim;
  su2double Total_Sens_Geo_local = 0.0;
  Total_Sens_Geo = 0.0;

 // for (iMarker = 0; iMarker < nMarker; iMarker++){
    Sens_Geo[iMarker] = 0.0;
    /* --- Loop over boundary markers to select those for Euler walls and NS walls --- */

    if(config->GetMarker_All_KindBC(iMarker) == EULER_WALL
       || config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX
       || config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL){

      for (iVertex = 0; iVertex < geometry->GetnVertex(iMarker); iVertex++){
        iPoint = geometry->vertex[iMarker][iVertex]->GetNode();
        Normal = geometry->vertex[iMarker][iVertex]->GetNormal();
        Prod = 0.0;
        Area = 0.0;
        for (iDim = 0; iDim < nDim; iDim++){
          /* --- retrieve the gradient calculated with AD -- */
          SensDim = node[iPoint]->GetSensitivity(iDim);

          /* --- calculate scalar product for projection onto the normal vector ---*/
          Prod += Normal[iDim]*SensDim;
          Area += Normal[iDim]*Normal[iDim];
        }
        Area = sqrt(Area);

        /* --- projection of the gradient
         *     calculated with AD onto the normal
         *     vector of the surface --- */
        //Sens = Prod;
        Sens=Prod/Area;

        /*--- Compute sensitivity for each surface point ---*/
        CSensitivity[iMarker][iVertex] = -Sens;
        if (geometry->node[iPoint]->GetFlip_Orientation())
          CSensitivity[iMarker][iVertex] = -CSensitivity[iMarker][iVertex];

        if (geometry->node[iPoint]->GetDomain()){
          Sens_Geo[iMarker] += Sens*Sens;
        }
      }
      Total_Sens_Geo_local += sqrt(Sens_Geo[iMarker]);

    }
 // }

#ifdef HAVE_MPI
  SU2_MPI::Allreduce(&Total_Sens_Geo_local,&Total_Sens_Geo,1,MPI_DOUBLE,MPI_SUM, MPI_COMM_WORLD);
#else
  Total_Sens_Geo = Total_Sens_Geo_local;
#endif
}

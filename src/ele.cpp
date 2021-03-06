/*!
 * \file ele.cpp
 * \brief ele class definition
 *
 * Each elements stores its solution and basic properties, like
 * element type, vertices, and polynomial order
 *
 * \author - Jacob Crabill
 *           Aerospace Computing Laboratory (ACL)
 *           Aero/Astro Department. Stanford University
 *
 * \version 0.0.1
 *
 * Flux Reconstruction in C++ (Flurry++) Code
 * Copyright (C) 2014 Jacob Crabill
 *
 */

#include "../include/ele.hpp"

#include "../include/polynomials.hpp"
#include "../include/flux.hpp"

using namespace std;

ele::ele()
{

}

ele::ele(int in_eType, int in_order, int in_ID, vector<point> &in_nodes, geo *in_Geo)
{
  eType = in_eType;
  order = in_order;
  ID = in_ID;
  Geo = in_Geo;

  nodes.clear();
  for (auto &n: in_nodes)
    nodes.push_back(n);

}

void ele::initialize(void)
{
  int fpt, face;

  switch(eType) {
  case(TRI):
    for (fpt=0; fpt<nFpts; fpt++) {
      face = fpt%(nFpts/3);
      switch(face) {
      case(0):
        tNorm_fpts[fpt][0] = 0;
        tNorm_fpts[fpt][1] = -1;
        break;
      case(1):
        tNorm_fpts[fpt][0] = 1.0/sqrt(2);
        tNorm_fpts[fpt][1] = 1.0/sqrt(2);
        break;
      case(2):
        tNorm_fpts[fpt][0] = -1;
        tNorm_fpts[fpt][1] = 0;
        break;
      }
    }
    break;
  case(QUAD):
    for (fpt=0; fpt<nFpts; fpt++) {
      face = fpt%(nFpts/3);
      switch(face) {
      case(0):
        tNorm_fpts[fpt][0] = 0;
        tNorm_fpts[fpt][1] = -1;
        break;
      case(1):
        tNorm_fpts[fpt][0] = 1;
        tNorm_fpts[fpt][1] = 0;
        break;
      case(2):
        tNorm_fpts[fpt][0] = 0;
        tNorm_fpts[fpt][1] = 1;
        break;
      case(3):
        tNorm_fpts[fpt][0] = -1;
        tNorm_fpts[fpt][1] = 0;
        break;
      }
    }
    break;
  }
}

void ele::setup(input *inParams, geo *inGeo)
{
  /* --- Basic Stuff --- */
  params = inParams;
  Geo = inGeo;

  order = params->order;
  nDims = params->nDims;

  loc_spts = Geo->getLocSpts(eType,order);
  loc_fpts = Geo->getLocFpts(eType,order);

  nSpts = loc_spts.size();
  nFpts = loc_fpts.size();

  pos_spts.resize(nSpts);
  pos_fpts.resize(nFpts);

  if (params->equation == ADVECTION_DIFFUSION) {
    nFields = 1;
  }else if (params->equation == NAVIER_STOKES) {
    nFields = nDims + 2;
  }

  /* --- Setup all data arrays --- */
  U_spts.setup(nSpts,nFields);
  U_fpts.setup(nFpts,nFields);
  U_mpts.setup(nNodes,nFields);
  Fn_fpts.setup(nFpts,nFields);
  dFn_fpts.setup(nFpts,nFields);

  switch (params->timeType) {
    case 0:
      nRKSteps = 1;
      break;
    case 4:
      nRKSteps = 4;
      break;
    default:
      FatalError("Time-advancement time not recognized.");
  }
  divF_spts.resize(nRKSteps);
  for (auto& dF:divF_spts) dF.setup(nSpts,nFields);


  dU_spts.resize(nDims);
  dU_fpts.resize(nDims);
  for (int dim=0; dim<nDims; dim++) {
    dU_spts[dim].setup(nSpts,nFields);
    dU_fpts[dim].setup(nFpts,nFields);
  }

  F_spts.resize(nDims);
  F_fpts.resize(nDims);
  dF_spts.resize(nDims);
  tdF_spts.resize(nDims);
  for (int i=0; i<nDims; i++) {
    F_spts[i].setup(nSpts,nFields);
    F_fpts[i].setup(nFpts,nFields);
    dF_spts[i].resize(nDims);
    tdF_spts[i].setup(nSpts,nFields);
    for (int j=0; j<nDims; j++) {
      dF_spts[i][j].setup(nSpts,nFields);
    }
  }

  detJac_spts.resize(nSpts);
  detJac_fpts.resize(nFpts);
  Jac_spts.resize(nSpts);
  Jac_fpts.resize(nFpts);
  JGinv_spts.resize(nSpts);
  for (auto& spt:Jac_spts) spt.setup(nDims,nDims);
  for (auto& fpt:Jac_fpts) fpt.setup(nDims,nDims);
  for (auto& spt:JGinv_spts) spt.setup(nDims,nDims);

  norm_fpts.setup(nFpts,nDims);
  tNorm_fpts.setup(nFpts,nDims);
  dA_fpts.resize(nFpts);

  gridVel_nodes.setup(nNodes,nDims);
  gridVel_spts.setup(nSpts,nDims);
  gridVel_fpts.setup(nFpts,nDims);
  gridVel_nodes.initializeToZero();
  gridVel_spts.initializeToZero();
  gridVel_fpts.initializeToZero();

  if (params->motion != 0) {
    nodesRK.resize(nRKSteps);
    for (auto &vec:nodesRK) {
      vec = nodes;
    }
  }

  tempF.setup(nDims,nFields);
  tempU.assign(nFields,0);

  /* --- Final Step: calculate physical->reference transforms --- */
  setShape_spts();
  setShape_fpts();
  setDShape_spts();
  setDShape_fpts();
  setTransformedNormals_fpts();
  calcTransforms();

  calcPosSpts();
  calcPosFpts();
  setPpts();
}

void ele::move(int step)
{
  if (params->motion == 1) {
    perturb();
  }

  updateTransforms();
  calcGridVelocity();
}

void ele::perturb(void)
{
  for (int iv=0; iv<nNodes; iv++) {
    /// Taken from Kui, AIAA-2010-5031-661
    nodesRK[0][iv].x = nodes[iv].x + 2*sin(pi*nodes[iv].x/10.)*sin(pi*nodes[iv].y/10.)*sin(2*pi*params->rkTime/10.);
    nodesRK[0][iv].y = nodes[iv].y + 2*sin(pi*nodes[iv].x/10.)*sin(pi*nodes[iv].y/10.)*sin(2*pi*params->rkTime/10.);
  }
}

void ele::calcGridVelocity(void)
{
  if (params->motion == 1) {
    for (int iv=0; iv<nNodes; iv++) {
      gridVel_nodes(iv,0) = 4.*pi/10.*sin(pi*nodes[iv].x/10.)*sin(pi*nodes[iv].y/10.)*cos(2*pi*params->rkTime/10.); // from Kui
      gridVel_nodes(iv,1) = 4.*pi/10.*sin(pi*nodes[iv].x/10.)*sin(pi*nodes[iv].y/10.)*cos(2*pi*params->rkTime/10.);
    }

    gridVel_spts.initializeToZero();
    for (int spt=0; spt<nSpts; spt++) {
      for (int iv=0; iv<nNodes; iv++) {
        for (int dim=0; dim<nDims; dim++) {
          gridVel_spts(spt,dim) += shape_spts(spt,iv)*gridVel_nodes(iv,dim);
        }
      }
    }

    gridVel_fpts.initializeToZero();
    for (int fpt=0; fpt<nFpts; fpt++) {
      for (int iv=0; iv<nNodes; iv++) {
        for (int dim=0; dim<nDims; dim++) {
          gridVel_fpts(fpt,dim) += shape_fpts(fpt,iv)*gridVel_nodes(iv,dim);
        }
      }
    }
    // replace with: calcGridVelocitySpts();
  }
}

void ele::setShape_spts(void)
{
  shape_spts.setup(nSpts,nNodes);

  for (int spt=0; spt<nSpts; spt++) {
    switch(eType) {
      case TRI:
        break;
      case QUAD:
        shape_quad(loc_spts[spt],shape_spts[spt]);
        break;
    }
  }
}

void ele::setShape_fpts(void)
{
  shape_fpts.setup(nFpts,nNodes);

  for (int fpt=0; fpt<nFpts; fpt++) {
    switch(eType) {
      case TRI:
        break;
      case QUAD:
        shape_quad(loc_fpts[fpt],shape_fpts[fpt]);
        break;
    }
  }
}

void ele::setDShape_spts(void)
{
  dShape_spts.resize(nSpts);
  for (auto& dS:dShape_spts) dS.setup(nNodes,nDims);

  for (int spt=0; spt<nSpts; spt++) {
    switch(eType) {
      case TRI:
        dshape_tri(loc_spts[spt], dShape_spts[spt]);
        break;
      case QUAD:
        dshape_quad(loc_spts[spt], dShape_spts[spt]);
        break;
      default:
        FatalError("Element type not yet implemented.")
    }
  }
}

void ele::setDShape_fpts(void)
{
  dShape_fpts.resize(nFpts);
  for (auto& dS:dShape_fpts) dS.setup(nDims,nDims);

  for (int fpt=0; fpt<nFpts; fpt++) {
    switch(eType) {
      case TRI:
        dshape_tri(loc_fpts[fpt], dShape_fpts[fpt]);
        break;
      case QUAD:
        dshape_quad(loc_fpts[fpt], dShape_fpts[fpt]);
        break;
      default:
        FatalError("Element type not yet implemented.")
    }
  }
}

void ele::setTransformedNormals_fpts(void)
{
  // Setting unit normal vector in the parent domain
  for (int fpt=0; fpt<nFpts; fpt++) {
    uint iFace = floor(fpt / (order+1));
    // Calculate shape derivatives [in the future, should pre-calculate & store]
    switch(eType) {
      case TRI:
        switch(iFace) {
          case 0:
            tNorm_fpts[fpt][0] = 0;
            tNorm_fpts[fpt][1] = -1;
            break;
          case 1:
            tNorm_fpts[fpt][0] = sqrt(2);
            tNorm_fpts[fpt][1] = sqrt(2);
            break;
          case 2:
            tNorm_fpts[fpt][0] = -1;
            tNorm_fpts[fpt][1] = 0;
            break;
        }
        break;
      case QUAD:
        // Face ordering for quads: Bottom, Right, Top, Left
        switch(iFace) {
          case 0:
            tNorm_fpts[fpt][0] = 0;
            tNorm_fpts[fpt][1] = -1;
            break;
          case 1:
            tNorm_fpts[fpt][0] = 1;
            tNorm_fpts[fpt][1] = 0;
            break;
          case 2:
            tNorm_fpts[fpt][0] = 0;
            tNorm_fpts[fpt][1] = 1;
            break;
          case 3:
            tNorm_fpts[fpt][0] = -1;
            tNorm_fpts[fpt][1] = 0;
            break;
        }
        break;
      default:
        FatalError("Element type not yet implemented.")
    }
  }
}

void ele::calcTransforms(void)
{
  /* --- Calculate Transformation at Solution Points --- */
  for (int spt=0; spt<nSpts; spt++) {
    Jac_spts[spt].initializeToZero();
    for (int i=0; i<nNodes; i++) {
      for (int dim1=0; dim1<nDims; dim1++) {
        for (int dim2=0; dim2<nDims; dim2++) {
          Jac_spts[spt][dim1][dim2] += dShape_spts[spt][i][dim2]*nodes[i][dim1];
        }
      }
    }

    if (nDims==2) {
      // Determinant of transformation matrix
      detJac_spts[spt] = Jac_spts[spt][0][0]*Jac_spts[spt][1][1]-Jac_spts[spt][1][0]*Jac_spts[spt][0][1];
      // Inverse of transformation matrix (times its determinant)
      JGinv_spts[spt][0][0] = Jac_spts[spt][1][1];  JGinv_spts[spt][0][1] =-Jac_spts[spt][0][1];
      JGinv_spts[spt][1][0] =-Jac_spts[spt][1][0];  JGinv_spts[spt][1][1] = Jac_spts[spt][0][0];
    }
    if (detJac_spts[spt]<0) FatalError("Negative Jacobian at solution points.");
  }

  /* --- Calculate Transformation at Flux Points --- */
  for (int fpt=0; fpt<nFpts; fpt++) {
    // Calculate transformation Jacobian matrix - [dx/dr, dx/ds; dy/dr, dy/ds]
    Jac_fpts[fpt].initializeToZero();
    for (int i=0; i<nNodes; i++) {
      for (int dim1=0; dim1<nDims; dim1++) {
        for (int dim2=0; dim2<nDims; dim2++) {
          Jac_fpts[fpt][dim1][dim2] += dShape_fpts[fpt][i][dim2]*nodes[i][dim1];
        }
      }
    }

    if (nDims==2) {
      detJac_fpts[fpt] = Jac_fpts[fpt][0][0]*Jac_fpts[fpt][1][1]-Jac_fpts[fpt][1][0]*Jac_fpts[fpt][0][1];
    }
    if (detJac_fpts[fpt]<0) FatalError("Negative Jacobian at solution points.");

    /* --- Calculate outward unit normal vector at flux point --- */
    // Transform face normal from reference to physical space [JGinv dot tNorm]
    norm_fpts[fpt][0] =  Jac_fpts[fpt][1][1]*tNorm_fpts[fpt][0] - Jac_fpts[fpt][1][0]*tNorm_fpts[fpt][1];
    norm_fpts[fpt][1] = -Jac_fpts[fpt][0][1]*tNorm_fpts[fpt][0] + Jac_fpts[fpt][0][0]*tNorm_fpts[fpt][1];

    // Store magnitude of face normal (equivalent to face area in finite-volume land)
    dA_fpts[fpt] = sqrt(norm_fpts[fpt][0]*norm_fpts[fpt][0] + norm_fpts[fpt][1]*norm_fpts[fpt][1]);

    // Normalize
    for (int dim=0; dim<nDims; dim++)
      norm_fpts[fpt][dim] /= dA_fpts[fpt];
  }
}

void ele::updateTransforms(void)
{
  /* --- Calculate Transformation at Solution Points --- */

  for (int spt=0; spt<nSpts; spt++) {
    Jac_spts[spt].initializeToZero();
    for (int i=0; i<nNodes; i++) {
      for (int dim1=0; dim1<nDims; dim1++) {
        for (int dim2=0; dim2<nDims; dim2++) {
          Jac_spts[spt][dim1][dim2] += dShape_spts[spt][i][dim2]*nodesRK[0][i][dim1];
        }
      }
    }

    if (nDims==2) {
      // Determinant of transformation matrix
      detJac_spts[spt] = Jac_spts[spt][0][0]*Jac_spts[spt][1][1]-Jac_spts[spt][1][0]*Jac_spts[spt][0][1];
      // Inverse of transformation matrix (times its determinant)
      JGinv_spts[spt][0][0] = Jac_spts[spt][1][1];  JGinv_spts[spt][0][1] =-Jac_spts[spt][0][1];
      JGinv_spts[spt][1][0] =-Jac_spts[spt][1][0];  JGinv_spts[spt][1][1] = Jac_spts[spt][0][0];
    }
    if (detJac_spts[spt]<0) FatalError("Negative Jacobian at solution points.");
  }

  /* --- Calculate Transformation at Flux Points --- */
  for (int fpt=0; fpt<nFpts; fpt++) {
    // Calculate transformation Jacobian matrix - [dx/dr, dx/ds; dy/dr, dy/ds]
    Jac_fpts[fpt].initializeToZero();
    for (int i=0; i<nNodes; i++) {
      for (int dim1=0; dim1<nDims; dim1++) {
        for (int dim2=0; dim2<nDims; dim2++) {
          Jac_fpts[fpt][dim1][dim2] += dShape_fpts[fpt][i][dim2]*nodesRK[0][i][dim1];
        }
      }
    }

    if (nDims==2) {
      detJac_fpts[fpt] = Jac_fpts[fpt][0][0]*Jac_fpts[fpt][1][1]-Jac_fpts[fpt][1][0]*Jac_fpts[fpt][0][1];
    }
    if (detJac_fpts[fpt]<0) FatalError("Negative Jacobian at solution points.");

    /* --- Calculate outward unit normal vector at flux point --- */
    // Transform face normal from reference to physical space [JGinv dot tNorm]
    norm_fpts[fpt][0] =  Jac_fpts[fpt][1][1]*tNorm_fpts[fpt][0] - Jac_fpts[fpt][1][0]*tNorm_fpts[fpt][1];
    norm_fpts[fpt][1] = -Jac_fpts[fpt][0][1]*tNorm_fpts[fpt][0] + Jac_fpts[fpt][0][0]*tNorm_fpts[fpt][1];

    // Store magnitude of face normal (equivalent to face area in finite-volume land)
    dA_fpts[fpt] = sqrt(norm_fpts[fpt][0]*norm_fpts[fpt][0] + norm_fpts[fpt][1]*norm_fpts[fpt][1]);

    // Normalize
    for (int dim=0; dim<nDims; dim++)
      norm_fpts[fpt][dim] /= dA_fpts[fpt];
  }
}

void ele::calcPosSpts(void)
{
  vector<double> shape;

  for (int spt=0; spt<nSpts; spt++) {
    getShape(loc_spts[spt], shape);
    pos_spts[spt].zero();
    for (int iv=0; iv<nNodes; iv++) {
      for (int dim=0; dim<nDims; dim++) {
        pos_spts[spt][dim] += shape[iv]*nodes[iv][dim];
      }
    }
  }
}

void ele::calcPosFpts(void)
{
  vector<double> shape;

  for (int fpt=0; fpt<nFpts; fpt++) {
    getShape(loc_fpts[fpt], shape);
    pos_fpts[fpt].zero();
    for (int iv=0; iv<nNodes; iv++) {
      for (int dim=0; dim<nDims; dim++) {
        pos_fpts[fpt][dim] += shape[iv]*nodes[iv][dim];
      }
    }
  }
}

void ele::updatePosSpts(void)
{
  vector<double> shape;

  for (int spt=0; spt<nSpts; spt++) {
    getShape(loc_spts[spt], shape);
    pos_spts[spt].zero();
    for (int iv=0; iv<nNodes; iv++) {
      for (int dim=0; dim<nDims; dim++) {
        pos_spts[spt][dim] += shape[iv]*nodesRK[0][iv][dim];
      }
    }
  }
}

void ele::updatePosFpts(void)
{
  vector<double> shape;

  for (int fpt=0; fpt<nFpts; fpt++) {
    getShape(loc_fpts[fpt], shape);
    pos_fpts[fpt].zero();
    for (int iv=0; iv<nNodes; iv++) {
      for (int dim=0; dim<nDims; dim++) {
        pos_fpts[fpt][dim] += shape[iv]*nodesRK[0][iv][dim];
      }
    }
  }
}


void ele::setInitialCondition()
{
  if (params->equation == NAVIER_STOKES) {
    double rho, vx, vy, p;
    double gamma = params->gamma;

    if (params->ic_type == 0) {
      /* --- Uniform "Freestream" solution --- */
      rho = params->rhoIC;
      vx = params->vxIC;
      vy = params->vyIC;
      p = params->pIC;
      for (int spt=0; spt<nSpts; spt++) {
        U_spts[spt][0] = rho;
        U_spts[spt][1] = rho * vx;
        U_spts[spt][2] = rho * vy;
        U_spts[spt][3] = p/(gamma - 1) + (0.5*rho*(vx*vx + vy*vy));
      }
    }
    else if (params->ic_type == 1) {
      /* --- Isentropic Vortex of strength eps centered at (0,0) --- */
      double eps, f, x, y;
      for (int spt=0; spt<nSpts; spt++) {
        eps = 5.0;
        x = pos_spts[spt][0];
        y = pos_spts[spt][1];

        f = 1.0 - (x*x + y*y);

        rho = pow(1. - eps*eps*(gamma-1.)/(8.*gamma*pi*pi)*exp(f), 1.0/(gamma-1.0));
        vx = 1. - eps*y / (2.*pi) * exp(f/2.);
        vy = 1. + eps*x / (2.*pi) * exp(f/2.);
        p = pow(rho,gamma);

        U_spts[spt][0] = rho;
        U_spts[spt][1] = rho * vx;
        U_spts[spt][2] = rho * vy;
        U_spts[spt][3] = p/(gamma - 1) + (0.5*rho*(vx*vx + vy*vy));
      }
    }
  }
  else if (params->equation == ADVECTION_DIFFUSION) {
    if (params->ic_type == 0) {
    /* --- Simple Gaussian bump centered at (0,0) --- */
    double r2;
    for (int spt=0; spt<nSpts; spt++) {
      r2 = pos_spts[spt][0]*pos_spts[spt][0] + pos_spts[spt][1]*pos_spts[spt][1];
      U_spts[spt][0] = exp(-r2);
    }
    }
    else if (params->ic_type == 1) {
      /* --- Test case for debugging - linear solution x+y over domain --- */
      for (int spt=0; spt<nSpts; spt++) {
        U_spts[spt][0] = pos_spts[spt][0]+pos_spts[spt][1];
      }
    }
  }
}

void ele::getShape(point loc, vector<double> &shape)
{
  if (eType == TRI) {
    shape_tri(loc, shape);
  }
  else if (eType == QUAD) {
    shape_quad(loc, shape);
  }
  else {
    FatalError("Element Type Not Supported.");
  }
}

void ele::calcInviscidFlux_spts()
{
  for (int spt=0; spt<nSpts; spt++) {

    inviscidFlux(U_spts[spt], tempF, params);

    if (params->motion) {
      /* --- Don't transform yet; that will be handled later --- */
      for (int i=0; i<nDims; i++) {
        for (int k=0; k<nFields; k++) {
          F_spts[i][spt][k] = tempF[i][k];
        }
      }
    }
    else {
      /* --- Transform back to reference domain --- */
      for (int i=0; i<nDims; i++) {
        for (int k=0; k<nFields; k++) {
          F_spts[i][spt][k] = 0.;
          for (int j=0; j<nDims; j++) {
            F_spts[i][spt][k] += JGinv_spts[spt][i][j]*tempF[j][k];
          }
        }
      }
    }

  }
}

void ele::calcViscousFlux_spts()
{
  for (int spt=0; spt<nSpts; spt++) {

    // TEMP HACK (inefficient, but will work fine)
    matrix<double> tempDU(nDims,nFields);
    for (int dim=0; dim<nDims; dim++) {
      for (int k=0; k<nFields; k++) {
        tempDU(dim,k) = dU_spts[dim](spt,k);
      }
    }
    viscousFlux(U_spts[spt], tempDU, tempF, params);

    /* --- Transform back to reference domain --- */
    for (int k=0; k<nFields; k++) {
      for (int i=0; i<nDims; i++) {
        for (int j=0; j<nDims; j++) {
          F_spts[i][spt][k] += JGinv_spts[spt][i][j]*tempF[j][k];
        }
      }
    }
  }
}

void ele::transformGradF_spts(int step)
{
  // The first 'nDim' of dF is the derivative, and the 2nd is the flux direction

  for (int spt=0; spt<nSpts; spt++) {
    double A = gridVel_spts(spt,1)*Jac_spts[spt](0,1) - gridVel_spts(spt,0)*Jac_spts[spt](1,1);
    double B = gridVel_spts(spt,0)*Jac_spts[spt](1,0) - gridVel_spts(spt,1)*Jac_spts[spt](0,0);
    for (int k=0; k<nFields; k++) {
      dF_spts[0][0](spt,k) =  dF_spts[0][0](spt,k)*Jac_spts[spt](1,1) - dF_spts[0][1](spt,k)*Jac_spts[spt](0,1) + dU_spts[0](spt,k)*A;
      dF_spts[1][1](spt,k) = -dF_spts[1][0](spt,k)*Jac_spts[spt](1,0) + dF_spts[1][1](spt,k)*Jac_spts[spt](0,0) + dU_spts[1](spt,k)*B;
      divF_spts[step](spt,k) = dF_spts[0][0](spt,k)+dF_spts[1][1](spt,k);
    }
  }
}

void ele::timeStepA(int step, double rkVal)
{
  for (int spt=0; spt<nSpts; spt++) {
    for (int i=0; i<nFields; i++) {
      U_spts[spt][i] = U0[spt][i] - rkVal * params->dt*divF_spts[step][spt][i]/detJac_spts[spt];
    }
  }
}

void ele::timeStepB(int step, double rkVal)
{
  for (int spt=0; spt<nSpts; spt++) {
    for (int i=0; i<nFields; i++) {
      U_spts[spt][i] -= rkVal * params->dt*divF_spts[step][spt][i]/detJac_spts[spt];
    }
  }
}


void ele::copyUspts_U0(void)
{
  U0 = U_spts;
}

void ele::copyU0_Uspts(void)
{
  U_spts = U0;
}

vector<double> ele::getPrimitives(uint spt)
{
  vector<double> V(nFields);

  if (params->equation == ADVECTION_DIFFUSION) {
    V[0] = U_spts[spt][0];
  }
  else if (params->equation == NAVIER_STOKES) {
    V[0] = U_spts[spt][0];
    V[1] = U_spts[spt][1]/V[0];
    V[2] = U_spts[spt][2]/V[0];
    V[3] = (params->gamma-1)*(U_spts[spt][3] - (0.5*(U_spts[spt][1]*U_spts[spt][1] + U_spts[spt][2]*U_spts[spt][2])/U_spts[spt][0]));
  }

  return V;
}

void ele::getPrimitivesPlot(matrix<double> &V)
{
  V.setup(nSpts+nFpts+nNodes,nFields);

  // Get solution at corner points
  for (int k=0; k<nFields; k++) {
    V[0][k]                     = U_mpts[0][k];
    V[order+2][k]               = U_mpts[1][k];
    V[(order+3)*(order+3)-1][k] = U_mpts[2][k];
    V[(order+3)*(order+2)][k]   = U_mpts[3][k];
  }

  // Get solution at flux points
  for (int i=0; i<order+1; i++) {
    for (int k=0; k<nFields; k++) {
      V[i+1][k]                     = U_fpts[i][k];               // Bottom
      V[(i+1)*(order+3)][k]         = U_fpts[nFpts-i-1][k];       // Left
      V[(i+2)*(order+3)-1][k]       = U_fpts[order+1+i][k];       // Right
      V[(order+3)*(order+2)+i+1][k] = U_fpts[3*(order+1)-i-1][k]; // Top
    }
  }

  // Get solution at solution points
  for (int i=0; i<order+1; i++) {
    for (int j=0; j<order+1; j++) {
      for (int k=0; k<nFields; k++) {
        int id = (i+1)*(order+3)+j+1;
        V[id][k] = U_spts[j+i*(order+1)][k];
      }
    }
  }

  if (params->equation == NAVIER_STOKES) {
    // Overwriting V, so be careful of order!
    for (uint i=0; i<V.getDim0(); i++) {
      V[i][3] = (params->gamma-1)*(V[i][3] - (0.5*(V[i][1]*V[i][1] + V[i][2]*V[i][2])/V[i][0]));
      V[i][1] = V[i][1]/V[i][0];
      V[i][2] = V[i][2]/V[i][0];
    }
  }
}

void ele::getGridVelPlot(matrix<double> &GV)
{
  GV.setup(nSpts+nFpts+nNodes,nDims);

  // Get solution at corner points
  for (int dim=0; dim<nDims; dim++) {
    GV[0][dim]                     = gridVel_nodes[0][dim];
    GV[order+2][dim]               = gridVel_nodes[1][dim];
    GV[(order+3)*(order+3)-1][dim] = gridVel_nodes[2][dim];
    GV[(order+3)*(order+2)][dim]   = gridVel_nodes[3][dim];
  }

  // Get solution at flux points
  for (int i=0; i<order+1; i++) {
    for (int dim=0; dim<nDims; dim++) {
      GV[i+1][dim]                     = gridVel_fpts[i][dim];               // Bottom
      GV[(i+1)*(order+3)][dim]         = gridVel_fpts[nFpts-i-1][dim];       // Left
      GV[(i+2)*(order+3)-1][dim]       = gridVel_fpts[order+1+i][dim];       // Right
      GV[(order+3)*(order+2)+i+1][dim] = gridVel_fpts[3*(order+1)-i-1][dim]; // Top
    }
  }

  // Get solution at solution points
  for (int i=0; i<order+1; i++) {
    for (int j=0; j<order+1; j++) {
      for (int dim=0; dim<nDims; dim++) {
        int id = (i+1)*(order+3)+j+1;
        GV[id][dim] = gridVel_spts[j+i*(order+1)][dim];
      }
    }
  }
}

vector<point> ele::getPpts(void)
{
  return pos_ppts;
}

void ele::setPpts(void)
{
  int nPts1D = order+3;
  pos_ppts.resize(nPts1D*nPts1D);

  // Get mesh (corner) points
  if (params->motion != 0) {
    pos_ppts[0*nPts1D+0]               = nodesRK[0][0];
    pos_ppts[0*nPts1D+order+2]         = nodesRK[0][1];
    pos_ppts[(order+2)*nPts1D+0]       = nodesRK[0][3];
    pos_ppts[(order+2)*nPts1D+order+2] = nodesRK[0][2];
  }
  else {
    pos_ppts[0*nPts1D+0]               = nodes[0];
    pos_ppts[0*nPts1D+order+2]         = nodes[1];
    pos_ppts[(order+2)*nPts1D+0]       = nodes[3];
    pos_ppts[(order+2)*nPts1D+order+2] = nodes[2];
  }

  // Get flux points
  for (int i=0; i<order+1; i++) {
    pos_ppts[0*nPts1D+i+1]         = pos_fpts[i];                // Bottom
    pos_ppts[(i+1)*nPts1D+0]       = pos_fpts[nFpts-i-1];        // Left
    pos_ppts[(i+1)*nPts1D+order+2] = pos_fpts[order+1+i];        // Right
    pos_ppts[(order+2)*nPts1D+i+1] = pos_fpts[3*(order+1)-i-1];  // Top
  }

  // Get solution at solution points
  for (int i=0; i<order+1; i++) {
    for (int j=0; j<order+1; j++) {
      pos_ppts[(i+1)*nPts1D+j+1] = pos_spts[j+i*(order+1)];
    }
  }
}

vector<double> ele::getResidual(int normType)
{
  vector<double> res(nFields,0);

  for (int spt=0; spt<nSpts; spt++) {
    for (int i=0; i<nFields; i++) {
      if (normType == 1) {
        res[i] += abs(divF_spts[0][spt][i]);
      }
      else if (normType == 2) {
        res[i] += divF_spts[0][spt][i]*divF_spts[0][spt][i];
      }
      else if (normType == 3) {
        // Infinity norm
        res[i] = max(abs(divF_spts[0][spt][i]),res[i]);
      }
    }
  }

  return res;
}

point ele::getPosSpt(uint spt)
{
  return pos_spts[spt];
}

point ele::getPosFpt(uint fpt)
{
  return pos_fpts[fpt];
}

uint ele::getNDims() const
{
  return nDims;
}

void ele::setNDims(int value)
{
  nDims = value;
}
uint ele::getNFields() const
{
  return nFields;
}

void ele::setNFields(int value)
{
  nFields = value;
}
uint ele::getNSpts() const
{
  return nSpts;
}

void ele::setNSpts(int value)
{
  nSpts = value;
}
uint ele::getNFpts() const
{
  return nFpts;
}

void ele::setNFpts(int value)
{
  nFpts = value;
}





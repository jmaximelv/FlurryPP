/*!
 * \file matrix.cpp
 * \brief Class for simplified matrix storage & manipulation
 *
 * \author - Jacob Crabill
 *           Aerospace Computing Laboratory (ACL)
 *           Aero/Astro Department. Stanford University
 *
 * \version 0.0.1
 *
 * Flux Reconstruction in C++ (Flurry++) Code
 * Copyright (C) 2014 Jacob Crabill.
 *
 */
#include "../include/matrix.hpp"

template<typename T>
matrix<T>::matrix()
{
  data.resize(0);
  dim0 = 0;
  dim1 = 0;
}

template<typename T>
matrix<T>::matrix(uint inDim0, uint inDim1)
{
  data.resize(inDim0*inDim1);
  dim0 = inDim0;
  dim1 = inDim1;
}

template<typename T>
matrix<T>::matrix(const matrix<T> &inMatrix)
{
  data = inMatrix.data;
  dim0 = inMatrix.dim0;
  dim1 = inMatrix.dim1;
}

template<typename T>
matrix<T> matrix<T>::operator=(const matrix<T> &inMatrix)
{
  data = inMatrix.data;
  dim0 = inMatrix.dim0;
  dim1 = inMatrix.dim1;
  return *this;
}

template<typename T>
void matrix<T>::setup(uint inDim0, uint inDim1)
{
  dim0 = inDim0;
  dim1 = inDim1;
  data.resize(inDim0*inDim1);
}

template<typename T>
void matrix<T>::addMatrix(matrix<T> &A, double a)
{
  if (A.dim0 != dim0 || A.dim1 != dim1)
    FatalError("Incompatible matrix sizes for addMatrix.");

  for (uint i=0; i<dim0; i++)
    for (uint j=0; j<dim1; j++)
      data[i*dim1+j] += a*A[i][j];
}

template<typename T>
T* matrix<T>::operator[](int inRow)
{
  if (inRow < (int)dim0 && inRow >= 0) {
    return &data[inRow*dim1];
  }
  else {
    FatalError("Attempted out-of-bounds access in matrix.");
  }
}

template<typename T>
T& matrix<T>::operator()(int i, int j)
{
  if (i<(int)dim0 && i>=0 && j<(int)dim1 && j>=0) {
    return data[i*dim1+j];
  }
  else {
    FatalError("Attempted out-of-bounds access in matrix.");
  }
}

template<typename T>
void matrix<T>::initializeToZero(void)
{
  for (uint idim=0; idim<dim0; idim++)
    for (uint jdim=0; jdim<dim1; jdim++)
      data[idim*dim1+jdim] = 0;
}

template<typename T>
void matrix<T>::initializeToValue(T val)
{
  for (uint idim=0; idim<dim0; idim++)
    for (uint jdim=0; jdim<dim1; jdim++)
      data[idim*dim1+jdim] = val;
}

template <typename T>
void matrix<T>::timesMatrix(matrix<T> &A, matrix<T> &B)
{
  uint i, j, k, p;
  p = A.dim1;

  if (A.dim0 != dim1) FatalError("Incompatible matrix sizes in matrix multiplication!");
  if (B.dim0 != dim0 || B.dim1 != A.dim1) B.setup(dim0, A.dim1);

  B.initializeToZero();

  for (i=0; i<dim0; i++) {
    for (j=0; j<dim1; j++) {
      for (k=0; k<p; k++) {
        B[i][k] += data[i*dim1+j]*A[j][k];
      }
    }
  }
}


template <typename T>
void matrix<T>::timesMatrixPlus(matrix<T> &A, matrix<T> &B)
{
  uint i, j, k, p;
  p = A.dim1;

  if (A.dim0 != dim1) FatalError("Incompatible matrix sizes in matrix multiplication!");
  if (B.dim0 != dim0 || B.dim1 != A.dim1) B.setup(dim0, A.dim1);

  for (i=0; i<dim0; i++) {
    for (j=0; j<dim1; j++) {
      for (k=0; k<p; k++) {
        B[i][k] += data[i*dim1+j]*A[j][k];
      }
    }
  }
}

template <typename T>
void matrix<T>::timesVector(vector<T> &A, vector<T> &B)
{
  uint i, j;

  //if (A.size() != dim1) FatalError("Incompatible vector size");
  if (B.size() != dim1) B.resize(dim1);

  for (i=0; i<dim0; i++) {
    B[i] = 0;
    for (j=0; j<dim1; j++) {
      B[i] += data[i*dim1+j]*A[j];
    }
  }
}

// Support for non-arithematic data types (pointers) - do nothing.
template<>
void matrix<double*>::addMatrix(matrix<double*> &, double ) {
  FatalError("matrix.addMatrix not supported for non-arithematic data types.");
}

template<>
void matrix<double*>::timesMatrix(matrix<double*> &, matrix<double*> &) {
  // incompatible - do nothing.
  FatalError("matrix.timesMatrix not supported for non-arithematic data types.");
}

template<>
void matrix<double*>::timesMatrixPlus(matrix<double*> &, matrix<double*> &) {
  // incompatible - do nothing.
  FatalError("matrix.timesMatrixPlus not supported for non-arithematic data types.");
}

template<>
void matrix<double*>::timesVector(vector<double*> &, vector<double*> &) {
  // incompatible - do nothing.
  FatalError("matrix.timesVector not supported for non-arithematic data types.");
}

template<typename T>
void matrix<T>::insertRow(vector<T> &vec, int rowNum)
{
  if (dim1!= 0 && vec.size()!=dim1) FatalError("Attempting to assign row of wrong size to matrix.");

  if (rowNum==-1 || rowNum==(int)dim0) {
    // Default action - add to end
    data.insert(data.end(),vec.begin(),vec.end());
  }else{
    // Insert at specified location
    data.insert(data.begin()+rowNum*dim1,vec.begin(),vec.end());
  }

  if (dim1==0) dim1=vec.size(); // This may not be needed (i.e. may never have dim1==0). need to verify how I set up dim0, dim1...
  dim0++;
}

template<typename T>
void matrix<T>::insertRow(T *vec, int rowNum, int length)
{
  if (dim1!=0 && length!=dim1) FatalError("Attempting to assign row of wrong size to matrix.");

  if (rowNum==-1 || rowNum==(int)dim0) {
    // Default action - add to end
    data.insert(data.end(),vec,vec+length);
  }else{
    // Insert at specified location
    data.insert(data.begin()+rowNum*dim1,vec,vec+length);
  }

  if (dim1==0) dim1=length;
  dim0++;
}

template<typename T>
void matrix<T>::addCol(void)
{
  typename vector<T>::iterator it;
  for (uint row=0; row<dim0; row++) {
    it = data.begin() + (row+1)*(dim1+1) - 1;
    data.insert(it,it-1,it);
  }
  dim1++;
}

template<typename T>
vector<T> matrix<T>::getRow(uint row)
{
  vector<T> out;
  out.assign(&data[row*dim1],&data[row*dim1]+dim1);
  return out;
}

template<typename T>
matrix<T> matrix<T>::getRows(vector<int> ind)
{
  matrix<T> out;
  for (auto& i:ind) out.insertRow(&data[i*dim1],-1,dim1);
  return out;
}

template<typename T>
vector<T> matrix<T>::getCol(int col)
{
  vector<T> out;
  for (uint i=0; i<dim0; i++) out.push_back(data[i*dim1+col]);
  return  out;
}

template<typename T>
void matrix<T>::print()
{
  for (uint i=0; i<dim0; i++) {
    for (uint j=0; j<dim1; j++) {
      std::cout << std::setw(15) << std::setprecision(10) << data[i*dim1+j] << " ";
    }
    cout << endl;
  }
}

template<typename T>
void matrix<T>::unique(matrix<T> &out, vector<int> &iRow)
{
  out.setup(0,0);

  // Setup vector for rows of final matrix that map to each row of initial matrix
  iRow.resize(dim0);
  iRow.assign(dim0,-1);

  /* --- For each row in the matrix, compare to all
     previous rows to get first unique occurence --- */
  typename vector<T>::iterator itI, itJ;
  for (uint i=0; i<dim0; i++) {
    itI = data.begin() + i*dim1;
    for (uint j=0; j<i; j++) {
      itJ = data.begin() + j*dim1;
      if (equal(itI,itI+dim1,itJ)) {
        iRow[i] = iRow[j];
        break;
      }
    }

    // If no duplicate found, put in 'out' matrix
    if (iRow[i]==-1) {
      out.insertRow(&data[i*dim1],-1,dim1);
      iRow[i] = out.getDim0() - 1;
    }
  }
}

template<typename T>
vector<T> matrix<T>::getData(void)
{
  return data;
}

// Fix for compiler to know which template types will be needed later (and therefore must now be compiled):
template class matrix<int>;
template class matrix<double>;
template class matrix<double*>;

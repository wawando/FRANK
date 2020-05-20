#include "hicma/operations/LAPACK.h"
#include "hicma/extension_headers/operations.h"

#include "hicma/classes/dense.h"
#include "hicma/classes/hierarchical.h"
#include "hicma/classes/matrix.h"
#include "hicma/classes/uniform_hierarchical.h"
#include "hicma/operations/BLAS.h"
#include "hicma/util/omm_error_handler.h"
#include "hicma/util/timer.h"

#ifdef USE_MKL
#include <mkl.h>
#else
#include <lapacke.h>
#endif
#include "yorel/yomm2/cute.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <tuple>
#include <utility>
#include <vector>


namespace hicma
{

std::tuple<MatrixProxy, MatrixProxy> getrf(Matrix& A) { return getrf_omm(A); }

define_method(MatrixPair, getrf_omm, (Hierarchical& A)) {
  Hierarchical L(A.dim[0], A.dim[1]);
  for (int64_t i=0; i<A.dim[0]; i++) {
    std::tie(L(i, i), A(i, i)) = getrf(A(i,i));
    for (int64_t i_c=i+1; i_c<L.dim[0]; i_c++) {
      L(i_c, i) = std::move(A(i_c, i));
      trsm(A(i,i), L(i_c,i), TRSM_UPPER, TRSM_RIGHT);
    }
    for (int64_t j=i+1; j<A.dim[1]; j++) {
      trsm(L(i,i), A(i,j), TRSM_LOWER);
    }
    for (int64_t i_c=i+1; i_c<L.dim[0]; i_c++) {
      for (int64_t k=i+1; k<A.dim[1]; k++) {
        gemm(L(i_c,i), A(i,k), A(i_c,k), -1, 1);
      }
    }
  }
  return {std::move(L), std::move(A)};
}

define_method(MatrixPair, getrf_omm, (Dense& A)) {
  timing::start("DGETRF");
  std::vector<int> ipiv(std::min(A.dim[0], A.dim[1]));
  LAPACKE_dgetrf(
    LAPACK_ROW_MAJOR,
    A.dim[0], A.dim[1],
    &A, A.stride,
    &ipiv[0]
  );
  Dense L(A.dim[0], A.dim[1]);
  for (int64_t i=0; i<A.dim[0]; i++) {
    for (int64_t j=0; j<i; j++) {
      L(i, j) = A(i, j);
      A(i, j) = 0;
    }
    L(i, i) = 1;
  }
  timing::stop("DGETRF");
  return {std::move(L), std::move(A)};
}

define_method(MatrixPair, getrf_omm, (UniformHierarchical& A)) {
  UniformHierarchical L(A.dim[0], A.dim[1]);
  // TODO This is a fairly instable way of copying the bases.
  // Later methods involvin LowRankShared will check for matching pointers to
  // make operations faster, and that might break depending on what happens
  // here and the order of set_basis operations.
  L.copy_col_basis(A);
  L.copy_row_basis(A);
  // TODO Assuming that no LowRankShared are on diagonal! Otherwise more
  // set_basis necessary.
  for (int64_t i=0; i<A.dim[0]; i++) {
    std::tie(L(i, i), A(i, i)) = getrf(A(i,i));
    trsm(A(i, i), L.get_row_basis(i), TRSM_UPPER, TRSM_RIGHT);
    trsm(L(i, i), A.get_col_basis(i), TRSM_LOWER);
    for (int64_t i_c=i+1; i_c<L.dim[0]; i_c++) {
      L(i_c, i) = std::move(A(i_c, i));
      L.set_row_basis(i_c, i);
    }
    for (int64_t i_c=i+1; i_c<L.dim[0]; i_c++) {
      for (int64_t k=i+1; k<A.dim[1]; k++) {
        gemm(L(i_c,i), A(i,k), A(i_c,k), -1, 1);
      }
      L.set_col_basis(i_c, i);
    }
  }
  return {std::move(L), std::move(A)};
}

define_method(MatrixPair, getrf_omm, (Matrix& A)) {
  omm_error_handler("getrf", {A}, __FILE__, __LINE__);
  std::abort();
}

} // namespace hicma

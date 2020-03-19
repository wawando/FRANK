#include "hicma/operations/LAPACK.h"
#include "hicma/extension_headers/operations.h"

#include "hicma/classes/node.h"
#include "hicma/classes/dense.h"
#include "hicma/util/timer.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#ifdef USE_MKL
#include <mkl.h>
#else
#include <lapacke.h>
#endif
#include "yorel/yomm2/cute.hpp"


namespace hicma
{

std::vector<int> geqp3(Node& A, Node& R) {
  return geqp3_omm(A, R);
}

// Fallback default, abort with error message
define_method(std::vector<int>, geqp3_omm, (Dense& A, Dense& R)) {
  timing::start("DGEQP3");
  assert(A.dim[1] == R.dim[1]);
  // TODO The 0 initial value is important! Otherwise axes are fixed and results
  // can be wrong. See netlib dgeqp3 reference.
  // However, much faster with -1... maybe better starting values exist?
  std::vector<int> jpvt(A.dim[1], 0);
  std::vector<double> tau(std::min(A.dim[0], A.dim[1]), 0);
  LAPACKE_dgeqp3(
    LAPACK_ROW_MAJOR,
    A.dim[0], A.dim[1],
    &A, A.stride,
    &jpvt[0], &tau[0]
  );
  // jpvt is 1-based, bad for indexing!
  for (int& i : jpvt) --i;
  for(int i=0; i<A.dim[0]; i++) {
    for(int j=0; j<A.dim[1]; j++) {
      if (j >= i) R(i, j) = A(i, j);
    }
  }
  timing::stop("DGEQP3");
  return jpvt;
}

// Fallback default, abort with error message
define_method(std::vector<int>, geqp3_omm, (Node& A, Node& R)) {
  std::cerr << "geqp3(";
  std::cerr << A.type() << "," << R.type();
  std::cerr << ") undefined." << std::endl;
  abort();
}

} // namespace hicma

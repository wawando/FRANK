#include "hicma/classes/dense.h"
#include "hicma/extension_headers/classes.h"

#include "hicma/classes/hierarchical.h"
#include "hicma/classes/low_rank.h"
#include "hicma/classes/matrix.h"
#include "hicma/classes/matrix_proxy.h"
#include "hicma/classes/nested_basis.h"
#include "hicma/operations/BLAS.h"
#include "hicma/operations/misc.h"
#include "hicma/util/omm_error_handler.h"
#include "hicma/util/pre_scheduler.h"
#include "hicma/util/print.h"
#include "hicma/util/timer.h"

#include "yorel/yomm2/cute.hpp"
using yorel::yomm2::virtual_;

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>


namespace hicma
{

Dense::Dense(const Dense& A)
: Matrix(A), dim{A.dim[0], A.dim[1]}, stride(A.dim[1]), rel_start{0, 0}
{
  timing::start("Dense cctor");
  (*data).resize(dim[0]*dim[1], 0);
  data_ptr = &(*data)[0];
  fill_dense_from(A, *this);
  timing::stop("Dense cctor");
}

Dense& Dense::operator=(const Dense& A) {
  timing::start("Dense copy assignment");
  Matrix::operator=(A);
  dim = A.dim;
  stride = A.stride;
  (*data).resize(dim[0]*dim[1], 0);
  rel_start = {0, 0};
  data_ptr = &(*data)[0];
  fill_dense_from(A, *this);
  timing::stop("Dense copy assignment");
  return *this;
}

Dense::Dense(const Matrix& A)
: Matrix(A), dim{get_n_rows(A), get_n_cols(A)}, stride(dim[1]),
  data(std::make_shared<std::vector<double>>(dim[0]*dim[1], 0)),
  rel_start{0, 0}, data_ptr(&(*data)[0])
{
  fill_dense_from(A, *this);
}

define_method(void, fill_dense_from, (const Hierarchical& A, Dense& B)) {
  timing::start("make_dense(H)");
  Hierarchical BH(B, A, false);
  for (int64_t i=0; i<A.dim[0]; i++) {
    for (int64_t j=0; j<A.dim[1]; j++) {
      fill_dense_from(A(i, j), BH(i, j));
    }
  }
  timing::stop("make_dense(H)");
}

define_method(void, fill_dense_from, (const LowRank& A, Dense& B)) {
  timing::start("make_dense(LR)");
  gemm(gemm(A.U, A.S), A.V, B, 1, 0);
  timing::stop("make_dense(LR)");
}

define_method(void, fill_dense_from, (const NestedBasis& A, Dense& B)) {
  timing::start("make_dense(NestedBasis)");
  // Only use transfer matrix if there are no children
  if (A.num_child_basis() == 0) {
    fill_dense_from(A.transfer_matrix, B);
    timing::stop("make_dense(NestedBasis)");
    return;
  }
  Hierarchical AtransH(
    A.transfer_matrix,
    A.is_col_basis() ? A.num_child_basis() : 1,
    A.is_row_basis() ? A.num_child_basis() : 1,
    false
  );
  Hierarchical BH(
    B,
    A.is_col_basis() ? A.num_child_basis() : 1,
    A.is_row_basis() ? A.num_child_basis() : 1,
    false
  );
  for (int64_t i=0; i<A.num_child_basis(); ++i) {
    if (A.is_col_basis()) {
      gemm(A[i], AtransH[i], BH[i], false, false, 1, 0);
    } else if (A.is_row_basis()) {
      gemm(AtransH[i], A[i], BH[i], false, false, 1, 0);
    }
  }
  timing::stop("make_dense(NestedBasis)");
}

define_method(void, fill_dense_from, (const Dense& A, Dense& B)) {
  assert(A.dim[0] == B.dim[0]);
  assert(A.dim[1] == B.dim[1]);
  add_copy_task(A, B);
}

define_method(void, fill_dense_from, (const Matrix& A, Matrix& B)) {
  omm_error_handler("fill_dense_from", {A, B}, __FILE__, __LINE__);
  std::abort();
}

declare_method(Dense&&, move_from_dense, (virtual_<Matrix&>))

Dense::Dense(MatrixProxy&& A)
: Dense(move_from_dense(A)) {}

define_method(Dense&&, move_from_dense, (Dense& A)) {
  return std::move(A);
}

define_method(Dense&&, move_from_dense, (Matrix& A)) {
  omm_error_handler("move_from_dense", {A}, __FILE__, __LINE__);
  std::abort();
}

Dense::Dense(int64_t n_rows, int64_t n_cols)
: dim{n_rows, n_cols}, stride(dim[1]) {
  timing::start("Dense alloc");
  (*data).resize(dim[0]*dim[1], 0);
  rel_start = {0, 0};
  data_ptr = &(*data)[0];
  timing::stop("Dense alloc");
}

Dense::Dense(
  void (*func)(
    Dense& A,
    const std::vector<std::vector<double>>& x,
    int64_t row_start, int64_t col_start
  ),
  const std::vector<std::vector<double>>& x,
  int64_t n_rows, int64_t n_cols,
  int64_t row_start, int64_t col_start
) : Dense(n_rows, n_cols) {
  add_kernel_task(func, *this, x, row_start, col_start);
}

Dense::Dense(
  const Dense& A,
  int64_t n_rows, int64_t n_cols, int64_t row_start, int64_t col_start,
  bool copy
) : dim{n_rows, n_cols} {
  if (copy) {
    stride = dim[1];
    data = std::make_shared<std::vector<double>>(n_rows*n_cols);
    rel_start = {0, 0};
    data_ptr = &(*data)[0];
    add_copy_task(A, *this, row_start, col_start);
  } else {
    assert(row_start+dim[0] <= A.dim[0]);
    assert(col_start+dim[1] <= A.dim[1]);
    stride = A.stride;
    data = A.data;
    rel_start = {A.rel_start[0]+row_start, A.rel_start[1]+col_start};
    data_ptr = &(*data)[rel_start[0]*stride+rel_start[1]];
  }
}

const Dense& Dense::operator=(const double a) {
  add_assign_task(*this, a);
  return *this;
}

double& Dense::operator[](int64_t i) {
  assert(dim[0] == 1 || dim[1] == 1);
  if (dim[0] == 1) {
    assert(i < dim[1]);
    return data_ptr[i];
  } else {
    assert(i < dim[0]);
    return data_ptr[i*stride];
  }
}

const double& Dense::operator[](int64_t i) const {
  assert(dim[0] == 1 || dim[1] == 1);
  if (dim[0] == 1) {
    assert(i < dim[1]);
    return data_ptr[i];
  } else {
    assert(i < dim[0]);
    return data_ptr[i*stride];
  }
}

double& Dense::operator()(int64_t i, int64_t j) {
  assert(i < dim[0]);
  assert(j < dim[1]);
  return data_ptr[i*stride+j];
}

const double& Dense::operator()(int64_t i, int64_t j) const {
  assert(i < dim[0]);
  assert(j < dim[1]);
  return data_ptr[i*stride+j];
}

double* Dense::operator&() { return data_ptr; }

const double* Dense::operator&() const { return data_ptr; }

Dense Dense::share() const { return Dense(*this, dim[0], dim[1], 0, 0, false); }

bool Dense::is_shared() const { return (data.use_count() > 1); }

bool Dense::is_shared_with(const Dense& A) const {
  bool shared = (data == A.data);
  shared &= (data_ptr == A.data_ptr);
  shared &= (rel_start == A.rel_start);
  shared &= (dim == A.dim);
  return shared;
}

bool Dense::is_submatrix() const {
  bool out = (rel_start == std::array<int64_t, 2>{0, 0});
  // TODO Think about int64_t!
  out &= (data->size() == uint64_t(dim[0] * dim[1]));
  return !out;
}

} // namespace hicma

#include "hicma/classes/low_rank.h"

#include "hicma/classes/dense.h"
#include "hicma/classes/hierarchical.h"
#include "hicma/classes/matrix.h"
#include "hicma/classes/matrix_proxy.h"
#include "hicma/classes/nested_basis.h"
#include "hicma/extension_headers/classes.h"
#include "hicma/operations/randomized_factorizations.h"
#include "hicma/operations/misc.h"
#include "hicma/util/omm_error_handler.h"

#include "yorel/yomm2/cute.hpp"
using yorel::yomm2::virtual_;

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>


namespace hicma
{

declare_method(LowRank&&, move_from_low_rank, (virtual_<Matrix&>))

LowRank::LowRank(MatrixProxy&& A)
: LowRank(move_from_low_rank(A)) {}

define_method(LowRank&&, move_from_low_rank, (LowRank& A)) {
  return std::move(A);
}

define_method(LowRank&&, move_from_low_rank, (Matrix& A)) {
  omm_error_handler("move_from_low_rank", {A}, __FILE__, __LINE__);
  std::abort();
}

LowRank::LowRank(int64_t n_rows, int64_t n_cols, int64_t k)
: dim{n_rows, n_cols}, rank(k)
{
  U = Dense(dim[0], k);
  S = Dense(k, k);
  V = Dense(k, dim[1]);
}

LowRank::LowRank(const Dense& A, int64_t k)
: Matrix(A), dim{A.dim[0], A.dim[1]} {
  // Rank with oversampling limited by dimensions
  rank = std::min(std::min(k+5, dim[0]), dim[1]);
  Dense U_full, V_full;
  std::tie(U_full, S, V_full) = rsvd(A, rank);
  U = resize(U_full, dim[0], k);
  V = resize(V_full, k, dim[1]);
  S = resize(S, k, k);
  rank = k;
}

void LowRank::mergeU(const LowRank& A, const LowRank& B) {
  assert(rank == A.rank + B.rank);
  Hierarchical U_new(1, 2);
  U_new[0] = share_basis(A.U);
  U_new[1] = share_basis(B.U);
  U = Dense(U_new);
}

void LowRank::mergeS(const LowRank& A, const LowRank& B) {
  assert(rank == A.rank + B.rank);
  for (int64_t i=0; i<A.rank; i++) {
    for (int64_t j=0; j<A.rank; j++) {
      S(i,j) = A.S(i,j);
    }
    for (int64_t j=0; j<B.rank; j++) {
      S(i,j+A.rank) = 0;
    }
  }
  for (int64_t i=0; i<B.rank; i++) {
    for (int64_t j=0; j<A.rank; j++) {
      S(i+A.rank,j) = 0;
    }
    for (int64_t j=0; j<B.rank; j++) {
      S(i+A.rank,j+A.rank) = B.S(i,j);
    }
  }
}

void LowRank::mergeV(const LowRank& A, const LowRank& B) {
  assert(rank == A.rank + B.rank);
  Hierarchical V_new(2, 1);
  V_new[0] = share_basis(A.V);
  V_new[1] = share_basis(B.V);
  V = Dense(V_new);
}

LowRank::LowRank(
  const Matrix& U, const Dense& S, const Matrix& V, bool copy_S
) : U(share_basis(U)), V(share_basis(V)),
    S(copy_S ? Dense(S) : S.share()),
    dim{get_n_rows(U), get_n_cols(V)}, rank(S.dim[0]) {}

} // namespace hicma

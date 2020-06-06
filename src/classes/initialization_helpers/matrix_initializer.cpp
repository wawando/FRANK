#include "hicma/classes/intitialization_helpers/matrix_initializer.h"

#include "hicma/classes/dense.h"
#include "hicma/classes/hierarchical.h"
#include "hicma/classes/low_rank.h"
#include "hicma/classes/shared_basis.h"
#include "hicma/classes/intitialization_helpers/basis_tracker.h"
#include "hicma/classes/intitialization_helpers/cluster_tree.h"
#include "hicma/functions.h"
#include "hicma/operations/BLAS.h"
#include "hicma/operations/LAPACK.h"

#include <algorithm>
#include <cstdint>


namespace hicma
{

MatrixInitializer::MatrixInitializer(
  void (*kernel)(
    Dense& A,
    const std::vector<std::vector<double>>& x,
    int64_t row_start, int64_t col_start
  ),
  const std::vector<std::vector<double>>& x,
  int64_t admis, int64_t rank,
  int basis_type
) : kernel(kernel), x(x), admis(admis), rank(rank), basis_type(basis_type) {}

void MatrixInitializer::fill_dense_representation(
  Dense& A,
  const ClusterTree& node
) const {
  kernel(A, x, node.rows.start, node.cols.start);
}

Dense MatrixInitializer::get_dense_representation(
  const ClusterTree& node
) const {
  Dense representation(node.rows.n, node.cols.n);
  kernel(representation, x, node.rows.start, node.cols.start);
  return representation;
}

Dense MatrixInitializer::make_block_row(const ClusterTree& node) const {
  // Create row block without admissible blocks
  int64_t n_cols = 0;
  std::vector<std::reference_wrapper<const ClusterTree>> admissible_blocks;
  for (const ClusterTree& block : node.get_block_row()) {
    if (is_admissible(block)) {
      admissible_blocks.emplace_back(block);
      n_cols += block.cols.n;
    }
  }
  Dense block_row(node.rows.n, n_cols);
  int64_t col_start = 0;
  for (const ClusterTree& block : admissible_blocks) {
    Dense part(block_row, block.rows.n, block.cols.n, 0, col_start);
    fill_dense_representation(part, block);
    col_start += block.cols.n;
  }
  return block_row;
}

Dense MatrixInitializer::make_block_col(const ClusterTree& node) const {
  // Create col block without admissible blocks
  int64_t n_rows = 0;
  std::vector<std::reference_wrapper<const ClusterTree>> admissible_blocks;
  for (const ClusterTree& block : node.get_block_col()) {
    if (is_admissible(block)) {
      admissible_blocks.emplace_back(block);
      n_rows += block.rows.n;
    }
  }
  Dense block_col(n_rows, node.cols.n);
  int64_t row_start = 0;
  for (const ClusterTree& block : admissible_blocks) {
    Dense part(block_col, block.rows.n, block.cols.n, row_start, 0);
    fill_dense_representation(part, block);
    row_start += block.rows.n;
  }
  return block_col;
}

LowRank MatrixInitializer::get_compressed_representation(
  const ClusterTree& node
) {
  LowRank out;
  if (basis_type == NORMAL_BASIS) {
    out = LowRank(get_dense_representation(node), rank);
  } else if (basis_type == SHARED_BASIS) {
    int64_t sample_size = std::min(std::min(rank+5, node.rows.n), node.cols.n);
    if (!col_basis.has_basis(node.rows)) {
      Dense block_row = make_block_row(node);
      Dense RN(
        random_uniform, std::vector<std::vector<double>>(),
        block_row.dim[1], sample_size
      );
      Dense Y = gemm(block_row, RN);
      Dense Q(Y.dim[0], Y.dim[1]);
      Dense R(Y.dim[1], Y.dim[1]);
      qr(Y, Q, R);
      Dense QtA = gemm(Q, block_row, 1, true, false);
      Dense Ub, _, __;
      std::tie(Ub, _, __) = svd(QtA);
      Dense U = gemm(Q, Ub);
      U.resize(U.dim[0], rank);
      col_basis[node.rows] = SharedBasis(std::move(U));
    }
    if (!row_basis.has_basis(node.cols)) {
      Dense block_col = make_block_col(node);
      Dense RN(
        random_uniform, std::vector<std::vector<double>>(),
        block_col.dim[1], sample_size
      );
      Dense Y = gemm(block_col, RN);
      Dense Q(Y.dim[0], Y.dim[1]);
      Dense R(Y.dim[1], Y.dim[1]);
      qr(Y, Q, R);
      Dense QtA = gemm(Q, block_col, 1, true, false);
      Dense _, __, V;
      std::tie(_, __, V) = svd(QtA);
      V.resize(rank, V.dim[1]);
      row_basis[node.cols] = SharedBasis(std::move(V));
    }
    Dense D = get_dense_representation(node);
    Dense S = gemm(
      gemm(col_basis[node.rows], D, 1, true, false), row_basis[node.cols],
      1, false, true
    );
    out = LowRank(col_basis[node.rows], S, row_basis[node.cols]);
  }
  return out;
}

bool MatrixInitializer::is_admissible(const ClusterTree& node) const {
  bool admissible = true;
  // Main admissibility condition
  admissible &= (node.dist_to_diag() > admis);
  // Vectors are never admissible
  admissible &= (node.rows.n > 1 && node.cols.n > 1);
  return admissible;
}

} // namespace hicma

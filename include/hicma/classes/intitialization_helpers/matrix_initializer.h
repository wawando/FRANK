#ifndef hicma_classes_initialization_helpers_matrix_initializer_h
#define hicma_classes_initialization_helpers_matrix_initializer_h

#include "hicma/classes/dense.h"
#include "hicma/classes/low_rank.h"

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>


namespace hicma
{

class ClusterTree;

enum { NORMAL_BASIS, SHARED_BASIS };

class MatrixInitializer {
 private:
  void (*kernel)(
    Dense& A,
    const std::vector<std::vector<double>>& x,
    int64_t row_start, int64_t col_start
  ) = nullptr;
  const std::vector<std::vector<double>>& x;
  int64_t admis;
  int64_t rank;
  std::map<std::tuple<int64_t, int64_t>, std::shared_ptr<Dense>> col_bases;
  std::map<std::tuple<int64_t, int64_t>, std::shared_ptr<Dense>> row_bases;
  int basis_type = NORMAL_BASIS;
 public:

  // Special member functions
  MatrixInitializer() = delete;

  ~MatrixInitializer() = default;

  MatrixInitializer(const MatrixInitializer& A) = delete;

  MatrixInitializer& operator=(const MatrixInitializer& A) = delete;

  MatrixInitializer(MatrixInitializer&& A) = delete;

  MatrixInitializer& operator=(MatrixInitializer&& A) = default;

  // Additional constructors
  MatrixInitializer(
    void (*kernel)(
      Dense& A, const std::vector<std::vector<double>>& x,
      int64_t row_start, int64_t col_start
    ),
    const std::vector<std::vector<double>>& x,
    int64_t admis, int64_t rank,
    int basis_type
  );

  // Utility methods
  virtual void fill_dense_representation(
    Dense& A, const ClusterTree& node
  ) const;

  virtual Dense get_dense_representation(const ClusterTree& node) const;

  Dense make_block_row(const ClusterTree& node) const;

  Dense make_block_col(const ClusterTree& node) const;

  virtual LowRank get_compressed_representation(const ClusterTree& node);

  bool is_admissible(const ClusterTree& node) const;
};

} // namespace hicma


#endif // hicma_classes_initialization_helpers_matrix_initializer_h

#ifndef hicma_classes_initialization_helpers_matrix_initializer_kernel_h
#define hicma_classes_initialization_helpers_matrix_initializer_kernel_h

#include "hicma/definitions.h"
#include "hicma/classes/dense.h"
#include "hicma/classes/initialization_helpers/matrix_initializer.h"

#include <cstdint>
#include <vector>


namespace hicma
{

class ClusterTree;
class IndexRange;

class MatrixInitializerKernel : public MatrixInitializer {
 private:
  void (*kernel)(
    double* A, uint64_t A_rows, uint64_t A_cols, uint64_t A_stride,
    const std::vector<std::vector<double>>& x,
    int64_t row_start, int64_t col_start
  ) = nullptr;
 public:

  // Special member functions
  MatrixInitializerKernel() = delete;

  ~MatrixInitializerKernel() = default;

  MatrixInitializerKernel(const MatrixInitializerKernel& A) = delete;

  MatrixInitializerKernel& operator=(const MatrixInitializerKernel& A) = delete;

  MatrixInitializerKernel(MatrixInitializerKernel&& A) = delete;

  MatrixInitializerKernel& operator=(MatrixInitializerKernel&& A) = delete;

  // Additional constructors
  MatrixInitializerKernel(
    void (*kernel)(
      double* A, uint64_t A_rows, uint64_t A_cols, uint64_t A_stride,
      const std::vector<std::vector<double>>& x,
      int64_t row_start, int64_t col_start
    ),
    const std::vector<std::vector<double>>& coords,
    double admis, int64_t rank,
    int admis_type
  );

  // Utility methods
  void fill_dense_representation(
    Dense& A, const ClusterTree& node
  ) const override;

  void fill_dense_representation(
    Dense& A, const IndexRange& row_range, const IndexRange& col_range
  ) const override;

  Dense get_dense_representation(const ClusterTree& node) const override;
};

} // namespace hicma


#endif // hicma_classes_initialization_helpers_matrix_initializer_kernel_h

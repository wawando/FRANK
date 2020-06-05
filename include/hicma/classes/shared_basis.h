#ifndef hicma_classes_shared_basis_h
#define hicma_classes_shared_basis_h

#include "hicma/classes/matrix.h"
#include "hicma/classes/matrix_proxy.h"

#include <memory>


namespace hicma
{

class Dense;

class SharedBasis : public Matrix {
 private:
  std::shared_ptr<Dense> representation;
 public:
  // Special member functions
  SharedBasis() = default;

  virtual ~SharedBasis() = default;

  SharedBasis(const SharedBasis& A);

  SharedBasis& operator=(const SharedBasis& A);

  SharedBasis(SharedBasis&& A) = default;

  SharedBasis& operator=(SharedBasis&& A) = default;

  // Constructors
  SharedBasis(std::shared_ptr<Dense> representation);

  // Utility methods
  SharedBasis share() const;

  std::shared_ptr<Dense> get_ptr() const;
};

MatrixProxy share_basis(const Matrix& A);

bool is_shared(const Matrix& A, const Matrix& B);

} // namespace hicma

#endif // hicma_classes_shared_basis_h

#include "hierarchical.h"

#include <lapacke.h>
#include <cblas.h>

namespace hicma {

  Dense::Dense() {
    dim[0]=0; dim[1]=0;
  }

  Dense::Dense(const int m) {
    dim[0]=m; dim[1]=1; data.resize(dim[0]);
  }

  Dense::Dense(const int m, const int n) {
    dim[0]=m; dim[1]=n; data.resize(dim[0]*dim[1]);
  }

  Dense::Dense(
               void (*func)(
                            std::vector<double>& data,
                            std::vector<double>& x,
                            const int& ni,
                            const int& nj,
                            const int& i_begin,
                            const int& j_begin
                            ),
               std::vector<double>& x,
               const int ni,
               const int nj,
               const int i_begin,
               const int j_begin,
               const int _i_abs,
               const int _j_abs,
               const int _level
               ) : Node(_i_abs,_j_abs,_level) {
    dim[0] = ni; dim[1] = nj;
    data.resize(dim[0]*dim[1]);
    func(data, x, ni, nj, i_begin, j_begin);
  }

  Dense::Dense(const Dense& A) : Node(A.i_abs,A.j_abs,A.level), data(A.data) {
    dim[0]=A.dim[0]; dim[1]=A.dim[1];
  }

  Dense::Dense(Dense&& A) {
    swap(*this, A);
  }

  Dense::Dense(const Dense* A) : Node(A->i_abs,A->j_abs,A->level), data(A->data) {
    dim[0]=A->dim[0]; dim[1]=A->dim[1];
  }

  Dense::Dense(const Block& A)
    : Node((*A.ptr).i_abs, (*A.ptr).j_abs, (*A.ptr).level) {
    assert(A.is(HICMA_DENSE));
    Dense& ref = static_cast<Dense&>(*A.ptr);
    dim[0]=ref.dim[0]; dim[1]=ref.dim[1];
    data = ref.data;
  }

  Dense* Dense::clone() const {
    return new Dense(*this);
  }

  void swap(Dense& A, Dense& B) {
    using std::swap;
    swap(A.data, B.data);
    swap(A.dim, B.dim);
    swap(A.i_abs, B.i_abs);
    swap(A.j_abs, B.j_abs);
    swap(A.level, B.level);
  }

  const Node& Dense::operator=(const double a) {
    for (int i=0; i<dim[0]*dim[1]; i++)
      data[i] = a;
    return *this;
  }

  const Node& Dense::operator=(const Node& A) {
    if (A.is(HICMA_DENSE)) {
      const Dense& B = static_cast<const Dense&>(A);
      dim[0] = B.dim[0]; dim[1] = B.dim[1];
      data.resize(dim[0]*dim[1]);
      data = B.data;
      return *this;
    } else {
      std::cerr << this->type() << " = " << A.type();
      std::cerr << " is undefined." << std::endl;
      return *this;
    }
  };

  const Node& Dense::operator=(Node&& A) {
    if (A.is(HICMA_DENSE)) {
      swap(*this, static_cast<Dense&>(A));
      return *this;
    } else {
      std::cerr << this->type() << " = " << A.type();
      std::cerr << " is undefined." << std::endl;
      return *this;
    }
  };

  const Dense& Dense::operator=(Dense A) {
    swap(*this, A);
    return *this;
  }

  const Node& Dense::operator=(Block A) {
    return *this = std::move(*A.ptr);
  }

  Dense Dense::operator-() const {
    Dense A(dim[0],dim[1]);
    for (int i=0; i<dim[0]*dim[1]; i++) A[i] = -data[i];
    return A;
  }

  Block Dense::operator+(const Node& A) const {
    Block B(*this);
    B += A;
    return B;
  }
  Block Dense::operator+(Block&& A) const {
    return *this + *A.ptr;
  }
  const Node& Dense::operator+=(const Node& A) {
    if (A.is(HICMA_DENSE)) {
      const Dense& B = static_cast<const Dense&>(A);
      assert(dim[0] == B.dim[0] && dim[1] == B.dim[1]);
      for (int i=0; i<dim[0]*dim[1]; i++) {
        (*this)[i] += B[i];
      }
      return *this;
    } else if (A.is(HICMA_LOWRANK)) {
      const LowRank& B = static_cast<const LowRank&>(A);
      assert(dim[0] == B.dim[0] && dim[1] == B.dim[1]);
      return *this += B.dense();
    } else {
      std::cerr << this->type() << " + " << A.type();
      std::cerr << " is undefined." << std::endl;
      return *this;
    }
  }
  const Node& Dense::operator+=(Block&& A) {
    return *this += *A.ptr;
  }

  Block Dense::operator-(const Node& A) const {
    Block B(*this);
    B -= A;
    return B;
  }

  Block Dense::operator-(Block&& A) const {
    return *this - *A.ptr;
  }

  const Node& Dense::operator-=(const Node& A) {
    if (A.is(HICMA_DENSE)) {
      const Dense& B = static_cast<const Dense&>(A);
      assert(dim[0] == B.dim[0] && dim[1] == B.dim[1]);
      for (int i=0; i<dim[0]*dim[1]; i++) {
        (*this)[i] -= B[i];
      }
      return *this;
    } else if (A.is(HICMA_LOWRANK)) {
      const LowRank& B = static_cast<const LowRank&>(A);
      assert(dim[0] == B.dim[0] && dim[1] == B.dim[1]);
      return *this -= B.dense();
    } else {
      std::cerr << this->type() << " - " << A.type();
      std::cerr << " is undefined." << std::endl;
      return *this;
    }
  }

  const Node& Dense::operator-=(Block&& A) {
    return *this -= *A.ptr;
  }

  Block Dense::operator*(const Node& A) const {
    if (A.is(HICMA_LOWRANK)) {
      const LowRank& B = static_cast<const LowRank&>(A);
      assert(dim[0] == B.dim[0] && dim[1] == B.dim[1]);
      LowRank C(B);
      C.U = *this * B.U;
      return C;
    } else if (A.is(HICMA_DENSE)) {
      const Dense& B = static_cast<const Dense&>(A);
      assert(dim[1] == B.dim[0]);
      Dense C(dim[0], B.dim[1]);
      if (B.dim[1] == 1) {
        cblas_dgemv(
                    CblasRowMajor,
                    CblasNoTrans,
                    dim[0],
                    dim[1],
                    1,
                    &data[0],
                    dim[1],
                    &B[0],
                    1,
                    0,
                    &C[0],
                    1
                    );
      }
      else {
        cblas_dgemm(
                    CblasRowMajor,
                    CblasNoTrans,
                    CblasNoTrans,
                    C.dim[0],
                    C.dim[1],
                    dim[1],
                    1,
                    &data[0],
                    dim[1],
                    &B[0],
                    B.dim[1],
                    0,
                    &C[0],
                    C.dim[1]
                    );
      }
      return C;
    } else {
      std::cerr << this->type() << " * " << A.type();
      std::cerr << " is undefined." << std::endl;
      return Block();
    }
  }

  Block Dense::operator*(Block&& A) const {
    return *this * *A.ptr;
  }

  double& Dense::operator[](const int i) {
    assert(i<dim[0]*dim[1]);
    return data[i];
  }

  const double& Dense::operator[](const int i) const {
    assert(i<dim[0]*dim[1]);
    return data[i];
  }

  double& Dense::operator()(const int i, const int j) {
    assert(i<dim[0] && j<dim[1]);
    return data[i*dim[1]+j];
  }

  const double& Dense::operator()(const int i, const int j) const {
    assert(i<dim[0] && j<dim[1]);
    return data[i*dim[1]+j];
  }

  const bool Dense::is(const int enum_id) const {
    return enum_id == HICMA_DENSE;
  }

  const char* Dense::type() const { return "Dense"; }

  void Dense::resize(int i) {
    dim[0]=i; dim[1]=1;
    data.resize(dim[0]*dim[1]);
  }

  void Dense::resize(int i, int j) {
    dim[0]=i; dim[1]=j;
    data.resize(dim[0]*dim[1]);
  }

  double Dense::norm() const {
    double l2 = 0;
    for (int i=0; i<dim[0]; i++) {
      for (int j=0; j<dim[1]; j++) {
        l2 += data[i*dim[1]+j] * data[i*dim[1]+j];
      }
    }
    return l2;
  }

  void Dense::print() const {
    for (int i=0; i<dim[0]; i++) {
      for (int j=0; j<dim[1]; j++) {
        std::cout << std::setw(20) << std::setprecision(15) << data[i*dim[1]+j] << ' ';
      }
      std::cout << std::endl;
    }
    std::cout << "----------------------------------------------------------------------------------" << std::endl;
  }

  void Dense::getrf() {
    std::vector<int> ipiv(std::min(dim[0],dim[1]));
    LAPACKE_dgetrf(LAPACK_ROW_MAJOR, dim[0], dim[1], &data[0], dim[1], &ipiv[0]);
  }

  void Dense::trsm(const Node& A, const char& uplo) {
    if (A.is(HICMA_DENSE)) {
      const Dense& B = static_cast<const Dense&>(A);
      if (dim[1] == 1) {
        switch (uplo) {
        case 'l' :
          cblas_dtrsm(CblasRowMajor, CblasLeft, CblasLower, CblasNoTrans, CblasUnit,
                      dim[0], dim[1], 1, &B[0], B.dim[1], &data[0], dim[1]);
          break;
        case 'u' :
          cblas_dtrsm(CblasRowMajor, CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit,
                      dim[0], dim[1], 1, &B[0], B.dim[1], &data[0], dim[1]);
          break;
        default :
          std::cerr << "Second argument must be 'l' for lower, 'u' for upper." << std::endl;
          abort();
        }
      }
      else {
        switch (uplo) {
        case 'l' :
          cblas_dtrsm(CblasRowMajor, CblasLeft, CblasLower, CblasNoTrans, CblasUnit,
                      dim[0], dim[1], 1, &B[0], B.dim[1], &data[0], dim[1]);
          break;
        case 'u' :
          cblas_dtrsm(CblasRowMajor, CblasRight, CblasUpper, CblasNoTrans, CblasNonUnit,
                      dim[0], dim[1], 1, &B[0], B.dim[1], &data[0], dim[1]);
          break;
        default :
          std::cerr << "Second argument must be 'l' for lower, 'u' for upper." << std::endl;
          abort();
        }
      }
    } else {
      std::cerr << this->type() << " /= " << A.type();
      std::cerr << " is undefined." << std::endl;
      abort();
    }
  }

  void Dense::gemm(const Node& A, const Node& B) {
    if (A.is(HICMA_DENSE)) {
      if (B.is(HICMA_DENSE)) {
        *this = *this - (A * B);
      } else if (B.is(HICMA_LOWRANK)) {
        *this = *this - (A * B);
      } else if (B.is(HICMA_HIERARCHICAL)) {
        std::cerr << this->type() << " -= " << A.type();
        std::cerr << " * " << B.type() << " is undefined." << std::endl;
        abort();
      }
    } else if (A.is(HICMA_LOWRANK)) {
      if (B.is(HICMA_DENSE)) {
        *this = *this - (A * B);
      } else if (B.is(HICMA_LOWRANK)) {
        *this = *this - (A * B);
      } else if (B.is(HICMA_HIERARCHICAL)) {
        std::cerr << this->type() << " -= " << A.type();
        std::cerr << " * " << B.type() << " is undefined." << std::endl;
        abort();
      }
    } else {
      std::cerr << this->type() << " -= " << A.type();
      std::cerr << " * " << B.type() << " is undefined." << std::endl;
      abort();
    }
  }
}

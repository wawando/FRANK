#include "dense.h"
#include "low_rank.h"
#include "hierarchical.h"

namespace hicma {
  LowRank::LowRank() {
    dim[0]=0; dim[1]=0; rank=0;
  }

  LowRank::LowRank(const LowRank &A) : U(A.U), S(A.S), V(A.V) {
    dim[0]=A.dim[0]; dim[1]=A.dim[1]; rank=A.rank;
  }

  LowRank::LowRank(const Dense &D, const int k) {
    int m = dim[0] = D.dim[0];
    int n = dim[1] = D.dim[1];
    rank = k;
    U.resize(m,k);
    S.resize(k,k);
    V.resize(k,n);
    gsl_matrix *D2 = gsl_matrix_calloc(m,n);
    gsl_matrix *U2 = gsl_matrix_calloc(m,k);
    gsl_matrix *S2 = gsl_matrix_calloc(k,k);
    gsl_matrix *V2 = gsl_matrix_calloc(n,k);
    for(int i=0; i<m; i++){
      for(int j=0; j<n; j++){
        D2->data[i*n+j] = D(i,j);
      }
    }
    gsl_matrix *RN = gsl_matrix_calloc(n,k);
    initialize_random_matrix(RN);
    gsl_matrix *Y = gsl_matrix_alloc(m,k);
    matrix_matrix_mult(D2, RN, Y);
    gsl_matrix *Q = gsl_matrix_alloc(m,k);
    QR_factorization_getQ(Y, Q);
    gsl_matrix *Bt = gsl_matrix_alloc(n,k);
    matrix_transpose_matrix_mult(D2,Q,Bt);
    gsl_matrix *Qhat = gsl_matrix_calloc(n,k);
    gsl_matrix *Rhat = gsl_matrix_calloc(k,k);
    compute_QR_compact_factorization(Bt,Qhat,Rhat);
    gsl_matrix *Uhat = gsl_matrix_alloc(k,k);
    gsl_vector *Sigmahat = gsl_vector_alloc(k);
    gsl_matrix *Vhat = gsl_matrix_alloc(k,k);
    gsl_vector *svd_work_vec = gsl_vector_alloc(k);
    gsl_matrix_memcpy(Uhat, Rhat);
    gsl_linalg_SV_decomp (Uhat, Vhat, Sigmahat, svd_work_vec);
    build_diagonal_matrix(Sigmahat, k, S2);
    matrix_matrix_mult(Q,Vhat,U2);
    matrix_matrix_mult(Qhat,Uhat,V2);
    for(int i=0; i<m; i++){
      for(int j=0; j<k; j++){
        U(i,j) = U2->data[i*k+j];
      }
    }
    for(int i=0; i<k; i++){
      for(int j=0; j<k; j++){
        S(i,j) = S2->data[i*k+j];
      }
    }
    for(int i=0; i<n; i++){
      for(int j=0; j<k; j++){
        V(j,i) = V2->data[i*k+j];
      }
    }
    gsl_matrix_free(D2);
    gsl_matrix_free(U2);
    gsl_matrix_free(S2);
    gsl_matrix_free(V2);
    gsl_matrix_free(RN);
    gsl_matrix_free(Y);
    gsl_matrix_free(Q);
    gsl_matrix_free(Bt);
    gsl_matrix_free(Qhat);
    gsl_matrix_free(Rhat);
    gsl_vector_free(Sigmahat);
    gsl_matrix_free(Uhat);
    gsl_matrix_free(Vhat);
    gsl_vector_free(svd_work_vec);
  }

  const LowRank& LowRank::operator=(const LowRank A) {
    dim[0]=A.dim[0]; dim[1]=A.dim[1]; rank=A.rank;
    U = A.U;
    S = A.S;
    V = A.V;
    return *this;
  }

  Dense LowRank::operator+=(const Dense& D) const {
    return U * S * V + D;
  }

  Dense LowRank::operator-=(const Dense& D) const {
    return U * S * V - D;
  }

  Dense LowRank::operator+(const Dense& D) const {
    return *this += D;
  }

  Dense LowRank::operator-(const Dense& D) const {
    return *this -= D;
  }

  LowRank LowRank::operator+=(const LowRank& A) const {
    assert(dim[0]==A.dim[0] && dim[1]==A.dim[1]);
    LowRank C;
    if (rank+A.rank >= dim[0]) {
      C = LowRank(U * S * V + A.U * A.S * A.V, rank);
    }
    else {
      C.resize(dim[0], dim[1], rank+A.rank);
      C.mergeU(*this,A);
      C.mergeS(*this,A);
      C.mergeV(*this,A);
    }
    return C;
  }

  LowRank LowRank::operator-=(const LowRank& A) const {
    assert(dim[0]==A.dim[0] && dim[1]==A.dim[1]);
    LowRank C;
    if (rank+A.rank >= dim[0]) {
      C = LowRank(U * S * V - A.U * A.S * A.V, rank);
    }
    else {
      C.resize(dim[0], dim[1], rank+A.rank);
      C.mergeU(*this,-A);
      C.mergeS(*this,-A);
      C.mergeV(*this,-A);
    }
    return C;
  }

  LowRank LowRank::operator+(const LowRank& A) const {
    return *this += A;
  }

  LowRank LowRank::operator-(const LowRank& A) const {
    return *this -= A;
  }

  /*
  LowRank LowRank::operator*(const Dense& D) {
    V = V * D;
    return *this;
  }

  LowRank LowRank::operator*(const LowRank& A) {
    S = S * (V * A.U) * A.S;
    return *this;
  }
  */

  LowRank LowRank::operator-() const {
    LowRank A(*this);
    A.U = -U;
    A.S = -S;
    A.V = -V;
    return A;
  }

  void LowRank::resize(int m, int n, int k) {
    dim[0]=m; dim[1]=n; rank=k;
    U.resize(m,k);
    S.resize(k,k);
    V.resize(k,n);
  }

  void LowRank::trsm(Dense& A, const char& uplo) {
    Dense D = this->dense();
    D.trsm(A, uplo);
    *this = LowRank(D, this->rank);
  }

  LowRank& LowRank::gemm(const Dense& A, const Dense& B) {
    Dense D = this->dense();
    D.gemm(A, B);
    *this = LowRank(D, this->rank);
    return *this;
  }

  LowRank& LowRank::gemm(const LowRank& A, const Dense& B) {
    Dense D = this->dense();
    D.gemm(A, B);
    *this = LowRank(D, this->rank);
    return *this;
  }

  LowRank& LowRank::gemm(const Dense& A, const LowRank& B) {
    Dense D = this->dense();
    D.gemm(A, B);
    *this = LowRank(D, this->rank);
    return *this;
  }

  LowRank& LowRank::gemm(const LowRank& A, const LowRank& B) {
    Dense D = this->dense();
    D.gemm(A, B);
    *this = LowRank(D, this->rank);
    return *this;
  }

  void LowRank::mergeU(const LowRank&A, const LowRank& B) {
    assert(rank == A.rank + B.rank);
    for (int i=0; i<dim[0]; i++) {
      for (int j=0; j<A.rank; j++) {
        U(i,j) = A.U(i,j);
      }
      for (int j=0; j<B.rank; j++) {
        U(i,j+A.rank) = B.U(i,j);
      }
    }
  }

  void LowRank::mergeS(const LowRank&A, const LowRank& B) {
    assert(rank == A.rank + B.rank);
    for (int i=0; i<A.rank; i++) {
      for (int j=0; j<A.rank; j++) {
        S(i,j) = A.S(i,j);
      }
      for (int j=0; j<B.rank; j++) {
        S(i,j+A.rank) = 0;
      }
    }
    for (int i=0; i<B.rank; i++) {
      for (int j=0; j<A.rank; j++) {
        S(i+A.rank,j) = 0;
      }
      for (int j=0; j<B.rank; j++) {
        S(i+A.rank,j+A.rank) = B.S(i,j);
      }
    }
  }

  void LowRank::mergeV(const LowRank&A, const LowRank& B) {
    assert(rank == A.rank + B.rank);
    for (int i=0; i<A.rank; i++) {
      for (int j=0; j<dim[1]; j++) {
        V(i,j) = A.V(i,j);
      }
    }
    for (int i=0; i<B.rank; i++) {
      for (int j=0; j<dim[1]; j++) {
        V(i+A.rank,j) = B.V(i,j);
      }
    }
  }

  Dense LowRank::dense() const {
    return (U * S * V);
  }
}

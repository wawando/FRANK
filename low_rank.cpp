#include "any.h"
#include "low_rank.h"
#include "hierarchical.h"
#include "print.h"

#include <cassert>
#include <iostream>
#include <random>
#include <algorithm>

namespace hicma {

  LowRank::LowRank() {
    dim[0]=0; dim[1]=0; rank=0;
  }

  LowRank::LowRank(
                   const int m,
                   const int n,
                   const int k,
                   const int i_abs,
                   const int j_abs,
                   const int level) {
    dim[0]=m; dim[1]=n; rank=k;
    U = Dense(m, k, i_abs, j_abs, level);
    S = Dense(k, k, i_abs, j_abs, level);
    V = Dense(k, n, i_abs, j_abs, level);
  }

  LowRank::LowRank(const Dense& A, const int k) : Node(A.i_abs,A.j_abs,A.level) {
    dim[0] = A.dim[0];
    dim[1] = A.dim[1];
    // Rank with oversampling limited by dimensions
    rank = std::min(std::min(k+5, dim[0]), dim[1]);
    U = Dense(dim[0], k, i_abs, j_abs, level);
    S = Dense(rank, rank, i_abs, j_abs, level);
    V = Dense(k, dim[1], i_abs, j_abs, level);
    Dense RN(dim[1],rank);
    std::mt19937 generator;
    std::normal_distribution<double> distribution(0.0, 1.0);
    for (int i=0; i<dim[1]*rank; i++) {
      RN[i] = distribution(generator); // RN = randn(n,k+p)
    }
    Dense Y(dim[0],rank);
    Y.gemm(A, RN, CblasNoTrans, CblasNoTrans, 1, 0); // Y = A * RN
    Dense Q(dim[0],rank);
    Dense R(rank,rank);
    Y.qr(Q, R); // [Q, R] = qr(Y)
    Dense Bt(dim[1],rank);
    Bt.gemm(A, Q, CblasTrans, CblasNoTrans, 1, 0); // B' = A' * Q
    Dense Qb(dim[1],rank);
    Dense Rb(rank,rank);
    Bt.qr(Qb,Rb); // [Qb, Rb] = qr(B')
    Dense Ur(rank,rank);
    Dense Vr(rank,rank);
    Rb.svd(Vr,S,Ur); // [Vr, S, Ur] = svd(Rb);
    Ur.resize(k,rank);
    U.gemm(Q, Ur, CblasNoTrans, CblasTrans, 1, 0); // U = Q * Ur'
    Vr.resize(rank,k);
    V.gemm(Vr, Qb, CblasTrans, CblasTrans, 1, 0); // V = Vr' * Qb'
    S.resize(k,k);
    rank = k;
  }

  LowRank::LowRank(const LowRank& A) : Node(A.i_abs,A.j_abs,A.level), U(A.U), S(A.S), V(A.V) {
     dim[0]=A.dim[0]; dim[1]=A.dim[1]; rank=A.rank;
  }

  LowRank::LowRank(LowRank&& A) {
    swap(*this, A);
  }

  LowRank::LowRank(const Any& _A, const int k) : Node(_A.ptr->i_abs, _A.ptr->j_abs, _A.ptr->level) {
    if (_A.is(HICMA_LOWRANK)) {
      *this = static_cast<const LowRank&>(*_A.ptr);
    } else if (_A.is(HICMA_DENSE)) {
      *this = LowRank(static_cast<const Dense&>(*_A.ptr), k);
    } else if (_A.is(HICMA_HIERARCHICAL)) {
      print_undefined(__func__, "Hierarchical", "LowRank");
      abort();
    }
  }

  LowRank* LowRank::clone() const {
    return new LowRank(*this);
  }

  void swap(LowRank& A, LowRank& B) {
    using std::swap;
    swap(A.U, B.U);
    swap(A.S, B.S);
    swap(A.V, B.V);
    swap(A.dim, B.dim);
    swap(A.i_abs, B.i_abs);
    swap(A.j_abs, B.j_abs);
    swap(A.level, B.level);
    swap(A.rank, B.rank);
  }

  const LowRank& LowRank::operator=(LowRank A) {
    swap(*this, A);
    return *this;
  }

  const LowRank& LowRank::operator+=(const LowRank& A) {
    assert(dim[0]==A.dim[0] && dim[1]==A.dim[1]);
    if (rank+A.rank >= dim[0]) {
      *this = LowRank(Dense(*this) + Dense(A), rank);
    } else {
      LowRank B(dim[0], dim[1], rank+A.rank, i_abs, j_abs, level);
      B.mergeU(*this, A);
      B.mergeS(*this, A);
      B.mergeV(*this, A);
      rank += A.rank;
      swap(U, B.U);
      swap(S, B.S);
      swap(V, B.V);
    }
    return *this;
  }

  bool LowRank::is(const int enum_id) const {
    return enum_id == HICMA_LOWRANK;
  }

  const char* LowRank::type() const { return "LowRank"; }

  double LowRank::norm() const {
    return Dense(*this).norm();
  }

  void LowRank::print() const {
    std::cout << "U : ------------------------------------------------------------------------------" << std::endl;
    U.print();
    std::cout << "S : ------------------------------------------------------------------------------" << std::endl;
    S.print();
    std::cout << "V : ------------------------------------------------------------------------------" << std::endl;
    V.print();
    std::cout << "----------------------------------------------------------------------------------" << std::endl;
  }

  void LowRank::transpose() {
    U.transpose();
    V.transpose();
    swap(U, V);
  }

  void LowRank::mergeU(const LowRank& A, const LowRank& B) {
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

  void LowRank::mergeS(const LowRank& A, const LowRank& B) {
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

  void LowRank::mergeV(const LowRank& A, const LowRank& B) {
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

  void LowRank::trsm(const Dense& A, const char& uplo) {
    switch (uplo) {
    case 'l' :
      U.trsm(A, uplo);
      break;
    case 'u' :
      V.trsm(A, uplo);
      break;
    }
  }

  void LowRank::trsm(const Hierarchical& A, const char& uplo) {
    switch (uplo) {
    case 'l' :
      {
        Hierarchical H(U, A.dim[0], 1);
        H.trsm(A, uplo);
        U = Dense(H);
        break;
      }
    case 'u' :
      {
        Hierarchical H(V, 1, A.dim[1]);
        H.trsm(A, uplo);
        V = Dense(H);
        break;
      }
    }
  }

  void LowRank::gemm(const Dense& A, const Dense& B, const double& alpha, const double& beta) {
    print_undefined(__func__, A.type(), B.type(), this->type());
    abort();
  }

  void LowRank::gemm(const Dense& A, const LowRank& B, const double& alpha, const double& beta) {
    LowRank C(B);
    C.U.gemm(A, B.U, alpha, 0);
    *this += C;
  }

  void LowRank::gemm(const Dense& A, const Hierarchical& B, const double& alpha, const double& beta) {
    print_undefined(__func__, A.type(), B.type(), this->type());
    abort();
  }

  void LowRank::gemm(const LowRank& A, const Dense& B, const double& alpha, const double& beta) {
   LowRank C(A);
    C.V.gemm(A.V, B, alpha, 0);
    *this += C;
  }

  void LowRank::gemm(const LowRank& A, const LowRank& B, const double& alpha, const double& beta) {
    LowRank C(A);
    C.V = B.V;
    Dense VxU(A.rank, B.rank);
    VxU.gemm(A.V, B.U, 1, 0);
    Dense SxVxU(A.rank, B.rank);
    SxVxU.gemm(A.S, VxU, 1, 0);
    C.S.gemm(SxVxU, B.S, alpha, 0);
    *this += C;
  }

  void LowRank::gemm(const LowRank& A, const Hierarchical& B, const double& alpha, const double& beta) {
    print_undefined(__func__, A.type(), B.type(), this->type());
    abort();
  }

  void LowRank::gemm(const Hierarchical& A, const Dense& B, const double& alpha, const double& beta) {
    print_undefined(__func__, A.type(), B.type(), this->type());
    abort();
  }

  void LowRank::gemm(const Hierarchical& A, const LowRank& B, const double& alpha, const double& beta) {
    print_undefined(__func__, A.type(), B.type(), this->type());
    abort();
  }

  void LowRank::gemm(const Hierarchical& A, const Hierarchical& B, const double& alpha, const double& beta) {
    print_undefined(__func__, A.type(), B.type(), this->type());
    abort();
  }

}

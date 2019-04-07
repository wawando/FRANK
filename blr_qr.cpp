#include "any.h"
#include "low_rank.h"
#include "hierarchical.h"
#include "functions.h"
#include "batch.h"
#include "print.h"
#include "timer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

using namespace hicma;

int main(int argc, char** argv) {
  int N = 8;
  int Nb = 4;
  int Nc = N / Nb;
  int rank = 2;
  std::vector<double> randx(N);
  Hierarchical A(Nc, Nc);
  Hierarchical D(Nc, Nc);
  Hierarchical Q(Nc, Nc);
  Hierarchical R(Nc, Nc);
  for(int i = 0; i < N; i++) {
    randx[i] = drand48();
  }
  std::sort(randx.begin(), randx.end());
  for (int ic=0; ic<Nc; ic++) {
    for (int jc=0; jc<Nc; jc++) {
      Dense Aij(laplace1d, randx, Nb, Nb, Nb*ic, Nb*jc);
      D(ic,jc) = Aij;
      if (std::abs(ic - jc) <= 1) {
        A(ic,jc) = Aij;
      }
      else {
        rsvd_push(A(ic,jc), Aij, rank);
      }
      //Fill R with zeros
      Dense Rij(Nb, Nb);
      R(ic, jc) = Rij;
    }
  }
  rsvd_batch();
  double diff = 0, norm = 0;
  for (int ic=0; ic<Nc; ic++) {
    for (int jc=0; jc<Nc; jc++) {
      if(A(ic,jc).is(HICMA_LOWRANK)) {
        diff += (Dense(A(ic,jc)) - D(ic,jc)).norm();
        norm += D(ic,jc).norm();
      }
    }
  }
  print("Compression Accuracy");
  print("Rel. L2 Error", std::sqrt(diff/norm), false);
  print("Time");
  Hierarchical _A(A); //Copy of A
  start("QR decomposition");
  for(int j = 0; j < Nc; j++) {
    Hierarchical Aj(Nc, 1);
    for(int i=0; i<Nc; i++) {
      Aj(i, 0) = A(i, j);
    }
    Hierarchical Qsj;
    Dense Rjj;
    Aj.blr_col_qr(Qsj, Rjj);
    R(j, j) = Rjj;
    //Copy column of Qsj to Q
    for(int i = 0; i < Nc; i++) {
      Q(i, j) = Qsj(i, 0);
    }
    //Transpose of Qsj to be used in processing next columns
    Hierarchical TrQsj(Qsj);
    TrQsj.transpose();
    //Process next columns
    for(int k = j + 1; k < Nc; k++) {
      //Take k-th column
      Hierarchical HAsk(Nc, 1);
      for(int i = 0; i < Nc; i++) {
        HAsk(i, 0) = A(i, k);
      }
      Dense DRjk(Nb, Nb);
      DRjk.gemm(TrQsj, HAsk, 1, 1); //Rjk = Q*j^T x A*k
      LowRank Rjk(DRjk, rank); 
      R(j, k) = Rjk;

      HAsk.gemm(Qsj, Rjk, -1, 1); //A*k = A*k - Q*j x Rjk
      for(int i = 0; i < Nc; i++) {
        A(i, k) = HAsk(i, 0);
      }
    }
  }
  stop("QR decomposition");
  printTime("-DGEQRF");
  printTime("-DGEMM");
  Dense QR(N, N);
  QR.gemm(Dense(Q), Dense(R), 1, 1);
  diff = (Dense(_A) - QR).norm();
  norm = _A.norm();
  print("Accuracy");
  print("Rel. L2 Error", std::sqrt(diff/norm), false);
  return 0;
}



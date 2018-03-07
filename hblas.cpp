#include "hblas.h"

namespace hicma {
  void getrf(boost::any& A) {
    if (A.type() == typeid(Dense)) {
      boost::any_cast<Dense&>(A).getrf();
    }
    else if (A.type() == typeid(Hierarchical)) {
      boost::any_cast<Hierarchical&>(A).getrf();
    }
    else {
      fprintf(stderr,"Data type must be Dense or Hierarchical.\n"); abort();
    }
  }

  void trsm(const boost::any& Aii, boost::any& Aij, const char& uplo) {
    if (Aii.type() == typeid(Dense)) {
      if (Aij.type() == typeid(Dense)) {
        boost::any_cast<Dense&>(Aij).trsm(boost::any_cast<const Dense&>(Aii), uplo);
      }
      else if (Aij.type() == typeid(LowRank)) {
        boost::any_cast<LowRank&>(Aij).trsm(boost::any_cast<const Dense&>(Aii), uplo);
      }
      else if (Aij.type() == typeid(Hierarchical)) {
        fprintf(stderr,"H /= D\n"); abort();
        //boost::any_cast<Hierarchical&>(Aij).trsm(boost::any_cast<const Dense&>(Aii), uplo);
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else if (Aii.type() == typeid(Hierarchical)) {
      if (Aij.type() == typeid(Dense)) {
        fprintf(stderr,"D /= H\n"); abort();
        //boost::any_cast<Dense&>(Aij).trsm(boost::any_cast<const Hierarchical&>(Aii), uplo);
      }
      else if (Aij.type() == typeid(LowRank)) {
        fprintf(stderr,"L /= H\n"); abort();
        //boost::any_cast<LowRank&>(Aij).trsm(boost::any_cast<const Hierarchical&>(Aii), uplo);
      }
      else if (Aij.type() == typeid(Hierarchical)) {
        boost::any_cast<Hierarchical&>(Aij).trsm(boost::any_cast<const Hierarchical&>(Aii), uplo);
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else {
      fprintf(stderr,"First value must be Dense or Hierarchical.\n"); abort();
    }
  }

  void gemm(const boost::any& A, const boost::any& B, boost::any& C) {
    if (A.type() == typeid(Dense)) {
      if (B.type() == typeid(Dense)) {
        if (C.type() == typeid(Dense)) {
          boost::any_cast<Dense&>(C).gemm(boost::any_cast<const Dense&>(A), boost::any_cast<const Dense&>(B));
        }
        else if (C.type() == typeid(LowRank)) {
          fprintf(stderr,"L += D * D\n"); abort();
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += D * D\n"); abort();
        }
        else {
          Dense D(boost::any_cast<const Dense&>(A).dim[0],boost::any_cast<const Dense&>(B).dim[1]);
          C = D;
          boost::any_cast<Dense&>(C).gemm(boost::any_cast<const Dense&>(A), boost::any_cast<const Dense&>(B));
        }
      }
      else if (B.type() == typeid(LowRank)) {
        if (C.type() == typeid(Dense)) {
          boost::any_cast<Dense&>(C).gemm(boost::any_cast<const Dense&>(A), boost::any_cast<const LowRank&>(B));
        }
        else if (C.type() == typeid(LowRank)) {
          boost::any_cast<LowRank&>(C).gemm(boost::any_cast<const Dense&>(A), boost::any_cast<const LowRank&>(B));
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += D * L\n"); abort();
        }
        else {
          LowRank D(boost::any_cast<const Dense&>(A).dim[0],boost::any_cast<const LowRank&>(B).dim[1],boost::any_cast<const LowRank&>(B).rank);
          C = D;
          boost::any_cast<LowRank&>(C).gemm(boost::any_cast<const Dense&>(A), boost::any_cast<const LowRank&>(B));
        }
      }
      else if (B.type() == typeid(Hierarchical)) {
        if (C.type() == typeid(Dense)) {
          fprintf(stderr,"D += D * H\n"); abort();
        }
        else if (C.type() == typeid(LowRank)) {
          fprintf(stderr,"L += D * H\n"); abort();
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += D * H\n"); abort();
        }
        else {
          fprintf(stderr,"D += D * H\n"); abort();
        }
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else if (A.type() == typeid(LowRank)) {
      if (B.type() == typeid(Dense)) {
        if (C.type() == typeid(Dense)) {
          boost::any_cast<Dense&>(C).gemm(boost::any_cast<const LowRank&>(A), boost::any_cast<const Dense&>(B));
        }
        else if (C.type() == typeid(LowRank)) {
          boost::any_cast<LowRank&>(C).gemm(boost::any_cast<const LowRank&>(A), boost::any_cast<const Dense&>(B));
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += L * D\n"); abort();
        }
        else {
          LowRank D(boost::any_cast<const LowRank&>(A).dim[0],boost::any_cast<const Dense&>(B).dim[1],boost::any_cast<const LowRank&>(A).rank);
          C = D;
          boost::any_cast<LowRank&>(C).gemm(boost::any_cast<const LowRank&>(A), boost::any_cast<const Dense&>(B));
        }
      }
      else if (B.type() == typeid(LowRank)) {
        if (C.type() == typeid(Dense)) {
          boost::any_cast<Dense&>(C).gemm(boost::any_cast<const LowRank&>(A), boost::any_cast<const LowRank&>(B));
        }
        else if (C.type() == typeid(LowRank)) {
          boost::any_cast<LowRank&>(C).gemm(boost::any_cast<const LowRank&>(A), boost::any_cast<const LowRank&>(B));
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += L * L\n"); abort();
        }
        else {
          LowRank D(boost::any_cast<const LowRank&>(A).dim[0],boost::any_cast<const LowRank&>(B).dim[1],boost::any_cast<const LowRank&>(A).rank);
          C = D;
          boost::any_cast<LowRank&>(C).gemm(boost::any_cast<const LowRank&>(A), boost::any_cast<const LowRank&>(B));
        }
      }
      else if (B.type() == typeid(Hierarchical)) {
        if (C.type() == typeid(Dense)) {
          fprintf(stderr,"D += L * H\n"); abort();
        }
        else if (C.type() == typeid(LowRank)) {
          fprintf(stderr,"L += L * H\n"); abort();
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += L * H\n"); abort();
        }
        else {
          fprintf(stderr,"L += L * H\n"); abort();
        }
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else if (A.type() == typeid(Hierarchical)) {
      if (B.type() == typeid(Dense)) {
        if (C.type() == typeid(Dense)) {
          fprintf(stderr,"D += H * D\n"); abort();
        }
        else if (C.type() == typeid(LowRank)) {
          fprintf(stderr,"L += H * D\n"); abort();
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += H * D\n"); abort();
        }
        else {
          fprintf(stderr,"D += H * D\n"); abort();
        }
      }
      else if (B.type() == typeid(LowRank)) {
        if (C.type() == typeid(Dense)) {
          fprintf(stderr,"D += H * L\n"); abort();
        }
        else if (C.type() == typeid(LowRank)) {
          fprintf(stderr,"L += H * L\n"); abort();
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += H * L\n"); abort();
        }
        else {
          fprintf(stderr,"L += H * L\n"); abort();
        }
      }
      else if (B.type() == typeid(Hierarchical)) {
        if (C.type() == typeid(Dense)) {
          fprintf(stderr,"D += H * H\n"); abort();
        }
        else if (C.type() == typeid(LowRank)) {
          fprintf(stderr,"L += H * H\n"); abort();
        }
        else if (C.type() == typeid(Hierarchical)) {
          fprintf(stderr,"H += H * H\n"); abort();
        }
        else {
          fprintf(stderr,"H += H * H\n"); abort();
        }
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else {
      fprintf(stderr,"First value must be Dense, LowRank or Hierarchical.\n"); abort();
    }
  }

  void add(const boost::any& A, const boost::any& B, boost::any& C) {
    if (A.type() == typeid(Dense)) {
      if (B.type() == typeid(Dense)) {
        assert(C.type() == typeid(Dense));
        boost::any_cast<Dense&>(C) = boost::any_cast<const Dense&>(A) + boost::any_cast<const Dense&>(B);
      }
      else if (B.type() == typeid(LowRank)) {
        assert(C.type() == typeid(Dense));
        boost::any_cast<Dense&>(C) = boost::any_cast<const Dense&>(A) + boost::any_cast<const LowRank&>(B);
      }
      else if (B.type() == typeid(Hierarchical)) {
        assert(C.type() == typeid(Hierarchical));
        fprintf(stderr,"H = D + H\n"); abort();
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else if (A.type() == typeid(LowRank)) {
      if (B.type() == typeid(Dense)) {
        assert(C.type() == typeid(Dense));
        boost::any_cast<Dense&>(C) = boost::any_cast<const LowRank&>(A) + boost::any_cast<const Dense&>(B);
      }
      else if (B.type() == typeid(LowRank)) {
        assert(C.type() == typeid(LowRank));
        boost::any_cast<LowRank&>(C) = boost::any_cast<const LowRank&>(A) + boost::any_cast<const LowRank&>(B);
      }
      else if (B.type() == typeid(Hierarchical)) {
        assert(C.type() == typeid(LowRank));
        fprintf(stderr,"L = L + H\n"); abort();
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else if (A.type() == typeid(Hierarchical)) {
      if (B.type() == typeid(Dense)) {
        assert(C.type() == typeid(Dense));
        fprintf(stderr,"D = H + D\n"); abort();
      }
      else if (B.type() == typeid(LowRank)) {
        assert(C.type() == typeid(LowRank));
        fprintf(stderr,"L = H + L\n"); abort();
      }
      else if (B.type() == typeid(Hierarchical)) {
        assert(C.type() == typeid(Hierarchical));
        fprintf(stderr,"H = H + H\n"); abort();
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else {
      fprintf(stderr,"First value must be Dense, LowRank or Hierarchical.\n"); abort();
    }
  }

  void sub(const boost::any& A, const boost::any& B, boost::any& C) {
    if (A.type() == typeid(Dense)) {
      if (B.type() == typeid(Dense)) {
        assert(C.type() == typeid(Dense));
        boost::any_cast<Dense&>(C) = boost::any_cast<const Dense&>(A) - boost::any_cast<const Dense&>(B);
      }
      else if (B.type() == typeid(LowRank)) {
        assert(C.type() == typeid(Dense));
        boost::any_cast<Dense&>(C) = boost::any_cast<const Dense&>(A) - boost::any_cast<const LowRank&>(B);
      }
      else if (B.type() == typeid(Hierarchical)) {
        assert(C.type() == typeid(Hierarchical));
        fprintf(stderr,"H = D - H\n"); abort();
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else if (A.type() == typeid(LowRank)) {
      if (B.type() == typeid(Dense)) {
        assert(C.type() == typeid(Dense));
        boost::any_cast<Dense&>(C) = boost::any_cast<const LowRank&>(A) - boost::any_cast<const Dense&>(B);
      }
      else if (B.type() == typeid(LowRank)) {
        assert(C.type() == typeid(LowRank));
        boost::any_cast<LowRank&>(C) = boost::any_cast<const LowRank&>(A) - boost::any_cast<const LowRank&>(B);
      }
      else if (B.type() == typeid(Hierarchical)) {
        assert(C.type() == typeid(LowRank));
        fprintf(stderr,"L = L - H\n"); abort();
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else if (A.type() == typeid(Hierarchical)) {
      if (B.type() == typeid(Dense)) {
        assert(C.type() == typeid(Dense));
        fprintf(stderr,"D = H - D\n"); abort();
      }
      else if (B.type() == typeid(LowRank)) {
        assert(C.type() == typeid(LowRank));
        fprintf(stderr,"L = H - L\n"); abort();
      }
      else if (B.type() == typeid(Hierarchical)) {
        assert(C.type() == typeid(Hierarchical));
        fprintf(stderr,"H = H - H\n"); abort();
      }
      else {
        fprintf(stderr,"Second value must be Dense, LowRank or Hierarchical.\n"); abort();
      }
    }
    else {
      fprintf(stderr,"First value must be Dense, LowRank or Hierarchical.\n"); abort();
    }
  }

  double norm(boost::any& A) {
    double l2 = 0;
    if (A.type() == typeid(Dense)) {
      l2 += boost::any_cast<Dense&>(A).norm();
    }
    else if (A.type() == typeid(LowRank)) {
      l2 += boost::any_cast<LowRank&>(A).norm();
    }
    else if (A.type() == typeid(Hierarchical)) {
      l2 += boost::any_cast<Hierarchical&>(A).norm();
    }
    else {
      fprintf(stderr,"Value must be Dense, LowRank or Hierarchical.\n"); abort();
    }
    return l2;
  }
}

#ifndef dense_h
#define dense_h
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace hicma {
  class Node;
  class LowRank;
  class Hierarchical;
  class Dense {
  public:
    std::vector<double> data;
    int dim[2];

    Dense();

    Dense(const int m);

    Dense(const int m, const int n);

    Dense(const Dense& A);

    Dense(
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
          const int j_begin
          );

    double& operator[](const int i);

    const double& operator[](const int i) const;

    double& operator()(const int i, const int j);

    const double& operator()(const int i, const int j) const;

    const Dense operator=(const double v);

    const Dense operator=(const Dense A);

    const Dense operator+=(const Dense& A);

    const Dense operator+=(const LowRank& A);

    const Dense operator+=(const Hierarchical& A);

    const Dense operator-=(const Dense& A);

    const Dense operator-=(const LowRank& A);

    const Dense operator-=(const Hierarchical& A);

    const Dense operator*=(const Dense& A);

    const LowRank operator*=(const LowRank& A);

    const Dense operator*=(const Hierarchical& A);

    Dense operator+(const Dense& A) const;

    Dense operator+(const LowRank& A) const;

    Dense operator+(const Hierarchical& A) const;

    Dense operator-(const Dense& A) const;

    Dense operator-(const LowRank& A) const;

    Dense operator-(const Hierarchical& A) const;

    Dense operator*(const Dense& A) const;

    LowRank operator*(const LowRank& A) const;

    Dense operator*(const Hierarchical& A) const;

    Dense operator-() const;

    void resize(int i);

    void resize(int i, int j);

    Dense extract(int i, int j, int ni, int nj);

    double norm();

    void print() const;

    void getrf();

    void trsm(const Dense& A, const char& uplo);

    void gemm(const Dense& A, const Dense& B);

    void gemm(const Dense& A, const LowRank& B);

    void gemm(const LowRank& A, const Dense& B);

    void gemm(const LowRank& A, const LowRank& B);
  };
}
#endif

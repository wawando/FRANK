#ifndef hierarchical_h
#define hierarchical_h
#include <memory>
#include "node.h"
#include "block_ptr.h"

namespace hicma {
  class Dense;
  class Hierarchical : public _Node {
  public:
    int dim[2];
    std::vector<Node> data;

    Hierarchical();

    Hierarchical(const int m);

    Hierarchical(const int m, const int n);

    Hierarchical(const Hierarchical& A);

    Hierarchical(const Hierarchical* A);

    Hierarchical(const HierarchicalPtr& A);

    Hierarchical(
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
                 const int rank,
                 const int nleaf,
                 const int admis=1,
                 const int ni_level=2,
                 const int nj_level=2,
                 const int i_begin=0,
                 const int j_begin=0,
                 const int i_abs=0,
                 const int j_abs=0,
                 const int level=0
                 );

    Hierarchical* clone() const override;

    const bool is(const int enum_id) const override;

    const char* is_string() const override;

    Node operator[](const int i);

    const Node& operator[](const int i) const;

    Node operator()(const int i, const int j);

    const Node& operator()(const int i, const int j) const;

    const _Node& operator=(const double a) override;

    const _Node& operator=(const _Node& A) override;

    const _Node& operator=(const Node& A) override;

    Node add(const Node& B) const override;

    Node sub(const Node& B) const override;

    Node mul(const Node& B) const override;

    double norm() const override;

    void print() const override;

    void getrf() override;

    void trsm(const Node& A, const char& uplo) override;

    void gemm(const Node& A, const Node& B);
  };
}
#endif

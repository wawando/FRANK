#ifndef hicma_classes_dense_h
#define hicma_classes_dense_h

#include "hicma/classes/node.h"

#include <vector>
#include <memory>

#include "yorel/multi_methods.hpp"

namespace hicma {

  class NodeProxy;
  class LowRank;
  class Hierarchical;

  class Dense : public Node {
  private:
    std::vector<double> data;
  protected:
    virtual double* get_pointer();

    virtual const double* get_pointer() const;
  public:
    MM_CLASS(Dense, Node);
    int dim[2] = {0, 0};
    int stride = 0;

    // Special member functions
    Dense();

    virtual ~Dense();

    Dense(const Dense& A);

    Dense& operator=(const Dense& A);

    Dense(Dense&& A);

    Dense& operator=(Dense&& A);

    // Overridden functions from Node
    virtual std::unique_ptr<Node> clone() const override;

    virtual std::unique_ptr<Node> move_clone() override;

    virtual const char* type() const override;

    // Explicit conversions using multiple-dispatch function.
    explicit Dense(const Node& A, bool only_node=false);

    // Conversion constructors
    Dense(NodeProxy&&);

    // Additional constructors
    Dense(
      int m, int n=1,
      int i_abs=0, int j_abs=0,
      int level=0
    );

    Dense(
      const Node& node,
      void (*func)(Dense& A, std::vector<double>& x),
      std::vector<double>& x
    );

    Dense(
      void (*func)(Dense& A, std::vector<double>& x),
      std::vector<double>& x,
      int ni, int nj=1,
      int i_begin=0, int j_begin=0,
      int i_abs=0, int j_abs=0,
      int level=0
    );

    Dense(
      void (*func)(
        std::vector<double>& data,
        std::vector<std::vector<double>>& x,
        const int& ni, const int& nj,
        const int& i_begin, const int& j_begin
      ),
      std::vector<std::vector<double>>& x,
      const int ni, const int nj,
      const int i_begin=0, const int j_begin=0,
      const int i_abs=0, const int j_abs=0,
      const int level=0
    );

    // Additional operators
    const Dense& operator=(const double a);

    Dense operator+(const Dense& A) const;

    Dense operator-(const Dense& A) const;

    const Dense& operator+=(const Dense& A);

    const Dense& operator-=(const Dense& A);

    const Dense& operator*=(const double);

    double& operator[](int i);

    const double& operator[](int i) const;

    double& operator()(int i, int j);

    const double& operator()(int i, int j) const;

    double* operator&();

    const double* operator&() const;

    // Utility methods
    int size() const;

    void resize(int dim0, int dim1);

    Dense transpose() const;

    void transpose();

    // Get part of other Dense
    Dense get_part(const Node& node) const;

  };

} // namespace hicma

#endif // hicma_classes_dense_h

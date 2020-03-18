#ifndef hicma_classes_uniform_hierarchical_h
#define hicma_classes_uniform_hierarchical_h

#include "hicma/classes/hierarchical.h"

#include <memory>
#include <vector>

#include "yorel/yomm2/cute.hpp"

namespace hicma
{

class NodeProxy;
class Dense;
class LowRankShared;

class UniformHierarchical : public Hierarchical {
private:
  std::vector<std::shared_ptr<Dense>> col_basis, row_basis;

  Dense make_row_block(
    int row,
    void (*func)(Dense& A, std::vector<double>& x),
    std::vector<double>& x,
    int admis
  );

  Dense make_col_block(
    int row,
    void (*func)(Dense& A, std::vector<double>& x),
    std::vector<double>& x,
    int admis
  );

  LowRankShared construct_shared_block_id(
    NodeProxy& child,
    std::vector<std::vector<int>>& selected_rows,
    std::vector<std::vector<int>>& selected_cols,
    void (*func)(Dense& A, std::vector<double>& x),
    std::vector<double>& x,
    int rank,
    int admis
  );

  LowRankShared construct_shared_block_svd(
    NodeProxy& child,
    void (*func)(Dense& A, std::vector<double>& x),
    std::vector<double>& x,
    int rank,
    int admis
  );
public:
  // Special member functions
  UniformHierarchical() = default;

  ~UniformHierarchical() = default;

  UniformHierarchical(const UniformHierarchical& A);

  UniformHierarchical& operator=(const UniformHierarchical& A) = default;

  UniformHierarchical(UniformHierarchical&& A) = default;

  UniformHierarchical& operator=(UniformHierarchical&& A) = default;

  // Overridden functions from Hierarchical
  std::unique_ptr<Node> clone() const override;

  std::unique_ptr<Node> move_clone() override;

  const char* type() const override;

  // Conversion constructors
  UniformHierarchical(NodeProxy&&);

  // Additional constructors
  UniformHierarchical(const Node& node, int ni_level, int nj_level);

  UniformHierarchical(
    const Node& node,
    void (*func)(Dense& A, std::vector<double>& x),
    std::vector<double>& x,
    int rank,
    int nleaf,
    int admis=1,
    int ni_level=2, int nj_level=2,
    bool use_svd=false
  );

  UniformHierarchical(
    void (*func)(Dense& A, std::vector<double>& x),
    std::vector<double>& x,
    int ni, int nj,
    int rank,
    int nleaf,
    int admis=1,
    int ni_level=2, int nj_level=2,
    bool use_svd=false,
    int i_begin=0, int j_begin=0,
    int i_abs=0, int j_abs=0,
    int level=0
  );

  // Additional indexing methods
  Dense& get_row_basis(int i);

  const Dense& get_row_basis(int i) const;

  Dense& get_col_basis(int j);

  const Dense& get_col_basis(int j) const;

  // Utiliry methods
  void copy_col_basis(const UniformHierarchical& A);

  void copy_row_basis(const UniformHierarchical& A);

  void set_col_basis(int i, int j);

  void set_row_basis(int i, int j);
};

register_class(UniformHierarchical, Hierarchical);

} // namespace hicma

#endif // hicma_classes_uniform_hierarchical_h

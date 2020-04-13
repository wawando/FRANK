#ifndef hicma_classes_low_rank_view_h
#define hicma_classes_low_rank_view_h

#include "hicma/classes/dense.h"
#include "hicma/classes/low_rank.h"

#include "yorel/yomm2/cute.hpp"

#include <memory>


namespace hicma
{

class IndexRange;
class Node;

class LowRankView : public LowRank {
 private:
  Dense _U, _S, _V;
 public:
  // Special member functions
  LowRankView() = default;

  ~LowRankView() = default;

  LowRankView(const LowRankView& A) = default;

  LowRankView& operator=(const LowRankView& A) = default;

  LowRankView(LowRankView&& A) = default;

  LowRankView& operator=(LowRankView&& A) = default;

  // Overridden functions from Node
  std::unique_ptr<Node> clone() const override;

  std::unique_ptr<Node> move_clone() override;

  const char* type() const override;

  Dense& U() override;
  const Dense& U() const override;

  Dense& S() override;
  const Dense& S() const override;

  Dense& V() override;
  const Dense& V() const override;

  // Constructors
  LowRankView(const LowRank& A);

  LowRankView(
    const IndexRange& row_range, const IndexRange& col_range, const LowRank& A);

  LowRankView(const Dense& U, const Dense& S, const Dense& V);
};

register_class(LowRankView, LowRank)

} // namespace hicma

#endif // hicma_classes_low_rank_view_h

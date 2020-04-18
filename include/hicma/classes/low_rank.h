#ifndef hicma_classes_low_rank_h
#define hicma_classes_low_rank_h

#include "hicma/classes/dense.h"
#include "hicma/classes/node.h"

#include "yorel/yomm2/cute.hpp"

#include <array>
#include <cstdint>
#include <memory>


namespace hicma
{

class ClusterTree;

class LowRank : public Node {
 private:
  Dense _U, _S, _V;
 public:
  std::array<int64_t, 2> dim = {0, 0};
  int64_t rank = 0;

  // Special member functions
  LowRank() = default;

  virtual ~LowRank() = default;

  LowRank(const LowRank& A) = default;

  LowRank& operator=(const LowRank& A) = default;

  LowRank(LowRank&& A) = default;

  LowRank& operator=(LowRank&& A) = default;

  // Overridden functions from Node
  virtual std::unique_ptr<Node> clone() const override;

  virtual std::unique_ptr<Node> move_clone() override;

  virtual const char* type() const override;

  // Getters and setters
  virtual Dense& U();
  virtual const Dense& U() const;

  virtual Dense& S();
  virtual const Dense& S() const;

  virtual Dense& V();
  virtual const Dense& V() const;

  // Additional constructors
  LowRank(int64_t m, int64_t n, int64_t k);

  LowRank(const Dense& A, int64_t k);

  LowRank(
    const ClusterTree& node,
    void (*func)(
      Dense& A, std::vector<double>& x, int64_t i_begin, int64_t j_begin
    ),
    std::vector<double>& x,
    int64_t k
  );

  void mergeU(const LowRank& A, const LowRank& B);

  void mergeS(const LowRank& A, const LowRank& B);

  void mergeV(const LowRank& A, const LowRank& B);

  // Utility methods
  LowRank get_part(const ClusterTree& node) const;

  LowRank(const ClusterTree& node, const LowRank& A);

  LowRank(const Dense& U, const Dense& S, const Dense& V);
};

register_class(LowRank, Node)

} // namespace hicma

#endif // hicma_classes_low_rank_h

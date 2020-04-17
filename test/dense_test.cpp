#include "hicma/hicma.h"

#include "gtest/gtest.h"
#include "yorel/yomm2/cute.hpp"

#include <cstdint>
#include <vector>


using namespace hicma;

TEST(DenseTest, ContructorHierarchical) {
  yorel::yomm2::update_methods();
  // Check whether the Dense(Hierarchical) constructor works correctly.
  int64_t N = 128;
  int64_t nblocks = 4;
  int64_t nleaf = N / nblocks;
  std::vector<double> randx = get_sorted_random_vector(N);
  // Construct single level all-dense hierarchical
  Hierarchical H(random_uniform, randx, N, N, 0, nleaf, nblocks, nblocks, nblocks);
  Dense D(H);
  // Check block-by-block and element-by-element if values match
  for (int64_t ib=0; ib<nblocks; ++ib) {
    for (int64_t jb=0; jb<nblocks; ++jb) {
      Dense D_compare = Dense(H(ib, jb));
      for (int64_t i=0; i<nleaf; ++i) {
        for (int64_t j=0; j<nleaf; ++j) {
          ASSERT_EQ(D(nleaf*ib+i, nleaf*jb+j), D_compare(i, j));
        }
      }
    }
  }
}

TEST(DenseTest, resize) {
  timing::start("Init");
  yorel::yomm2::update_methods();
  int64_t N = 4092;
  std::vector<double> vec;
  Dense D(random_normal, vec, N, N);
  Dense D_compare(D);
  timing::stopAndPrint("Init");
  timing::start("Resize");
  D.resize(N-N/8, N-N/8);
  timing::stopAndPrint("Resize");
  timing::start("Check results");
  for (int64_t i=0; i<N-N/8; ++i) {
    for (int64_t j=0; j<N-N/8; ++j) {
      ASSERT_EQ(D(i, j), D_compare(i, j));
    }
  }
  timing::stopAndPrint("Check results");
}

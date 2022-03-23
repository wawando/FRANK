#include <cstdint>
#include <vector>
#include <string>
#include <tuple>

#include "hicma/hicma.h"
#include "gtest/gtest.h"


class HierarchicalFixedRankTest
    : public testing::TestWithParam<std::tuple<int64_t, int64_t, int64_t, double, int>> {
 protected:
  void SetUp() override {
    hicma::initialize();
    std::tie(n_rows, nleaf, rank, admis, admis_type) = GetParam();
    n_cols = n_rows; // Assume square matrix
    nb_row = 2;
    nb_col = 2;
    randx_A.emplace_back(hicma::get_sorted_random_vector(std::max(n_rows, n_cols)));
  }
  int64_t n_rows, n_cols, nb_row, nb_col, nleaf, rank;
  double admis;
  int admis_type;
  std::vector<std::vector<double>> randx_A;
};

void expect_uniform_rank(hicma::Hierarchical& A, int64_t rank) {
  for(int64_t i=0; i<A.dim[0]; i++) {
    for(int64_t j=0; j<A.dim[1]; j++) {
      if(hicma::type(A(i, j)) == "LowRank") {
        hicma::LowRank Aij(std::move(A(i, j)));
        EXPECT_EQ(Aij.rank, rank);
      }
      else if(hicma::type(A(i, j)) == "Hierarchical") {
        hicma::Hierarchical Aij(std::move(A(i, j)));
        expect_uniform_rank(Aij, rank);
      }
    }
  }
}


TEST_P(HierarchicalFixedRankTest, ConstructionByKernel) {
  hicma::Hierarchical A(hicma::laplacend, randx_A, n_rows, n_cols,
                        rank, nleaf, admis, nb_row, nb_col, admis_type);
  expect_uniform_rank(A, rank);
}

TEST_P(HierarchicalFixedRankTest, ConstructionByDenseMatrix) {
  hicma::Dense D(hicma::laplacend, randx_A, n_rows, n_cols);
  hicma::Hierarchical A(std::move(D), rank, nleaf, admis,
                        nb_row, nb_col, 0, 0, randx_A, admis_type);
  expect_uniform_rank(A, rank);
}

INSTANTIATE_TEST_SUITE_P(HierarchicalTest, HierarchicalFixedRankTest,
                         testing::Combine(testing::Values(128, 256),
                                          testing::Values(16, 32),
                                          testing::Values(4, 8),
                                          testing::Values(0.0, 0.5, 1.0, 2.0),
                                          testing::Values(POSITION_BASED_ADMIS, GEOMETRY_BASED_ADMIS)
                                          ));


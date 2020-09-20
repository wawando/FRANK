#ifndef hicma_starpu_data_handler_h
#define hicma_starpu_data_handler_h

#include "starpu.h"

#include <memory>
#include <vector>


namespace hicma
{

class IndexRange;

class DataHandler {
 private:
  std::shared_ptr<std::vector<double>> data;
  std::vector<std::vector<starpu_data_handle_t>> splits;
  starpu_data_handle_t handle;
  bool is_child = false;
 public:
  // Special member functions
  DataHandler() = default;

  virtual ~DataHandler();

  DataHandler(const DataHandler& A) = delete;

  DataHandler& operator=(const DataHandler& A) = delete;

  DataHandler(DataHandler&& A) = delete;

  DataHandler& operator=(DataHandler&& A) = delete;

  DataHandler(int64_t n_rows, int64_t n_cols, double val=0);

  DataHandler(
    std::shared_ptr<std::vector<double>> data,
    starpu_data_handle_t handle
  );

  double& operator[](int64_t i);

  const double& operator[](int64_t i) const;

  uint64_t size() const;

  std::vector<std::shared_ptr<DataHandler>> split(
    const std::vector<IndexRange>& row_ranges,
    const std::vector<IndexRange>& col_ranges
  );

  starpu_data_handle_t get_handle() const;

  std::vector<starpu_data_handle_t>& get_last_split();
};

} // namespace hicma

#endif // hicma_starpu_data_handler_h

#include "hicma/util/initialize.h"

#include "hicma/classes.h"

#include "yorel/yomm2/cute.hpp"


namespace hicma
{

// Register all classes for the open multi methods
register_class(Matrix)
register_class(Dense, Matrix)
register_class(LowRank, Matrix)
register_class(Hierarchical, Matrix)
register_class(NestedBasis, Matrix)

class Runtime {
 public:
  void start() {
  // Update virtual tables for open multi methods
    yorel::yomm2::update_methods();
  }
};

static Runtime runtime;

void initialize() { runtime.start(); }

} // namespace hicma

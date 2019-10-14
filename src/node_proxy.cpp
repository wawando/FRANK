#include "hicma/node_proxy.h"
#include "hicma/low_rank.h"
#include "hicma/hierarchical.h"
#include "hicma/operations.h"

#include <iostream>

namespace hicma {

  NodeProxy::NodeProxy() = default;

  NodeProxy::NodeProxy(const NodeProxy& A) : ptr(A.ptr->clone()) {}

  NodeProxy::NodeProxy(const Node& A) : ptr(A.clone()) {}

  NodeProxy::~NodeProxy() = default;

  void swap(NodeProxy& A, NodeProxy& B){
    A.ptr.swap(B.ptr);
  }

  const NodeProxy& NodeProxy::operator=(NodeProxy A) {
    this->ptr = std::move(A.ptr);
    return *this;
  }

  const char* NodeProxy::type() const { return ptr->type(); }

  double NodeProxy::norm() const {
    return ptr->norm();
  }

  void NodeProxy::print() const {
    ptr->print();
  }

  void NodeProxy::transpose() {
    ptr->transpose();
  }

  // void NodeProxy::geqrt(NodeProxy& T) {
  //   if(T.is(HICMA_DENSE)) {
  //     ptr->geqrt(static_cast<Dense&>(*T.ptr));
  //   }
  //   else if(T.is(HICMA_HIERARCHICAL)) {
  //     ptr->geqrt(static_cast<Hierarchical&>(*T.ptr));
  //   }
  //   else {
  //     std::cerr << "Input matrix for geqrt must be Hierarchical or Dense." << std::endl;
  //     abort();
  //   }
  // }

}

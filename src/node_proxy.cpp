#include "hicma/node_proxy.h"
#include "hicma/low_rank.h"
#include "hicma/hierarchical.h"
#include "hicma/operations.h"

#include <cassert>
#include <iostream>

namespace hicma {

  NodeProxy::NodeProxy() = default;

  NodeProxy::NodeProxy(const NodeProxy& A) : ptr(A->clone()) {}

  NodeProxy::NodeProxy(const Node& A) : ptr(A.clone()) {}

  NodeProxy::~NodeProxy() = default;

  const Node* NodeProxy::operator->() const {
    return ptr.get();
  }

  Node* NodeProxy::operator->() {
    return ptr.get();
  }

  NodeProxy::operator const Node& () const {
    assert(ptr.get() != nullptr);
    return *ptr;
  }

  NodeProxy::operator Node& () {
    assert(ptr.get() != nullptr);
    return *ptr;
  }

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

}

#include "hicma/node.h"

#include <iostream>

#include "yorel/multi_methods.hpp"

namespace hicma {

  Node::Node() : i_abs(0), j_abs(0), level(0) { MM_INIT(); }

  Node::Node(
             const int i_abs,
             const int j_abs,
             const int level)
    : i_abs(i_abs), j_abs(j_abs), level(level) { MM_INIT(); }

  Node::Node(const Node& A)
    : i_abs(A.i_abs), j_abs(A.j_abs), level(A.level) { MM_INIT(); }

  Node::~Node() {};

  Node* Node::clone() const {
    return new Node(*this);
  }

  Node* Node::move_clone() {
    return new Node(std::move(*this));
  }

  const Node& Node::operator=(Node&& A) {
    return *this;
  }

  const char* Node::type() const { return "Node"; }

}

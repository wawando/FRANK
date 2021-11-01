#include "hicma/util/pre_scheduler.h"

#include "hicma/classes/dense.h"
#include "hicma/classes/hierarchical.h"
#include "hicma/classes/initialization_helpers/basis_tracker.h"
#include "hicma/operations/BLAS.h"
#include "hicma/operations/LAPACK.h"
#include "hicma/operations/misc.h"
#include "hicma/util/omm_error_handler.h"

#include "hicma_private/starpu_data_handler.h"

#include "starpu.h"
#include "yorel/yomm2/cute.hpp"
using yorel::yomm2::virtual_;

#ifdef USE_MKL
#include <mkl.h>
#else
#include <cblas.h>
#include <lapacke.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>


namespace hicma
{

class Task {
 public:
  // TODO Remove these and let tasks have individual arguments!
  std::vector<Dense> constant;
  std::vector<Dense> modified;
  starpu_task* task;

  // Special member functions
  Task() = default;

  virtual ~Task() = default;

  Task(const Task& A) = default;

  Task& operator=(const Task& A) = default;

  Task(Task&& A) = default;

  Task& operator=(Task&& A) = default;

  // Execute the task
  virtual void submit() = 0;

 protected:
  Task(
    std::vector<std::reference_wrapper<const Dense>> constant_,
    std::vector<std::reference_wrapper<Dense>> modified_
  ) {
    for (size_t i=0; i<constant_.size(); ++i) {
      constant.push_back(constant_[i].get().shallow_copy());
    }
    for (size_t i=0; i<modified_.size(); ++i) {
      modified.push_back(modified_[i].get().shallow_copy());
    }
  }

  starpu_data_handle_t get_handle(const Dense& A) {
    return A.data->get_handle();
  }

  DataHandler& get_handler(const Dense& A) { return *A.data;}
};

std::list<std::shared_ptr<Task>> tasks;
bool schedule_started = false;
bool is_tracking = false;

void add_task(std::shared_ptr<Task> task) {
  if (schedule_started) {
    tasks.push_back(task);
  } else {
    task->submit();
  }
}

struct kernel_args {
  void (*kernel)(
    double* A, uint64_t A_rows, uint64_t A_cols, uint64_t A_stride,
    const std::vector<std::vector<double>>& x,
    int64_t row_start, int64_t col_start
  ) = nullptr;
  const std::vector<std::vector<double>>& x;
  int64_t row_start, col_start;
};

void kernel_cpu_starpu_interface(void* buffers[], void* cl_args) {
  double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  struct kernel_args* args = (kernel_args*)cl_args;
  args->kernel(
    A, A_dim0, A_dim1, A_stride, args->x, args->row_start, args->col_start
  );
}

struct starpu_codelet kernel_cl;

class Kernel_task : public Task {
 public:
  kernel_args args;
  Kernel_task(
    void (*kernel)(
      double* A, uint64_t A_rows, uint64_t A_cols, uint64_t A_stride,
      const std::vector<std::vector<double>>& x,
      int64_t row_start, int64_t col_start
    ),
    Dense& A, const std::vector<std::vector<double>>& x,
    int64_t row_start, int64_t col_start
  ) : Task({}, {A}), args{kernel, x, row_start, col_start} {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &kernel_cl;
      task->cl_arg = &args;
      task->cl_arg_size = sizeof(args);
      task->handles[0] = get_handle(A);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "kernel_task");
    } else {
      args.kernel(
        &A, A.dim[0], A.dim[1], A.stride, args.x, args.row_start, args.col_start
    );
    }
  }
};

void add_kernel_task(
  void (*kernel)(
    double* A, uint64_t A_rows, uint64_t A_cols, uint64_t A_stride,
    const std::vector<std::vector<double>>& x,
    int64_t row_start, int64_t col_start
  ),
  Dense& A, const std::vector<std::vector<double>>& x,
  int64_t row_start, int64_t col_start
) {
  add_task(std::make_shared<Kernel_task>(kernel, A, x, row_start, col_start));
}

struct copy_args { int64_t row_start, col_start; };

void copy_cpu_func(
  const double* A, uint64_t A_stride,
  double* B, uint64_t B_dim0, uint64_t B_dim1, uint64_t B_stride,
  copy_args& args
) {
  if (args.row_start == 0 && args.col_start == 0) {
    for (uint64_t i=0; i<B_dim0; i++) {
      for (uint64_t j=0; j<B_dim1; j++) {
        B[i*B_stride+j] = A[i*A_stride+j];
      }
    }
  } else {
    for (uint64_t i=0; i<B_dim0; i++) {
      for (uint64_t j=0; j<B_dim1; j++) {
        B[i*B_stride+j] = A[(args.row_start+i)*A_stride+args.col_start+j];
      }
    }
  }
}

void copy_cpu_starpu_interface(void* buffers[], void* cl_args) {
  const double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  double* B = (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t B_dim0 = STARPU_MATRIX_GET_NY(buffers[1]);
  uint64_t B_dim1 = STARPU_MATRIX_GET_NX(buffers[1]);
  uint64_t B_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  struct copy_args* args = (copy_args*)cl_args;
  copy_cpu_func(A, A_stride, B, B_dim0, B_dim1, B_stride, *args);
}

struct starpu_codelet copy_cl;


class Copy_task : public Task {
 public:
  copy_args args;
  Copy_task(const Dense& A, Dense& B, int64_t row_start=0, int64_t col_start=0)
  : Task({A}, {B}), args{row_start, col_start} {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &copy_cl;
      task->cl_arg = &args;
      task->cl_arg_size = sizeof(args);
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(B);
    }
  }

  void submit() override {
    const Dense& A = constant[0];
    Dense& B = modified[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "copy_task");
    } else {
      copy_cpu_func(&A, A.stride, &B, B.dim[0], B.dim[1], B.stride, args);
    }
  }
};

void add_copy_task(
  const Dense& A, Dense& B, int64_t row_start, int64_t col_start
) {
  add_task(std::make_shared<Copy_task>(A, B, row_start, col_start));
}

void transpose_cpu_func(
  const double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  double* B, uint64_t B_stride
) {
  for (uint64_t i=0; i<A_dim0; i++) {
    for (uint64_t j=0; j<A_dim1; j++) {
      B[j*B_stride+i] = A[i*A_stride+j];
    }
  }
}

void transpose_cpu_starpu_interface(void* buffers[], void*) {
  const double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  double* B = (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t B_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  transpose_cpu_func(A, A_dim0, A_dim1, A_stride, B, B_stride);
}

struct starpu_codelet transpose_cl;


class Transpose_task : public Task {
 public:
  Transpose_task(const Dense& A, Dense& B) : Task({A}, {B}) {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &transpose_cl;
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(B);
    }
  }

  void submit() override {
    const Dense& A = constant[0];
    Dense& B = modified[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "transpose_task");
    } else {
      transpose_cpu_func(&A, A.dim[0], A.dim[1], A.stride, &B, B.stride);
    }
  }
};

void add_transpose_task(const Dense& A, Dense& B) {
  add_task(std::make_shared<Transpose_task>(A, B));
}

struct assign_args { double value; };

void assign_cpu_func(
  double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  assign_args& args
) {
  for (uint64_t i=0; i<A_dim0; i++) {
    for (uint64_t j=0; j<A_dim1; j++) {
      A[i*A_stride+j] = args.value;
    }
  }
}

void assign_cpu_starpu_interface(void* buffers[], void* cl_args) {
  double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  struct assign_args* args = (assign_args*)cl_args;
  assign_cpu_func(A, A_dim0, A_dim1, A_stride, *args);
}

struct starpu_codelet assign_cl;


class Assign_task : public Task {
 public:
  assign_args args;
  Assign_task(Dense& A, double value) : Task({}, {A}), args{value} {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &assign_cl;
      task->cl_arg = &args;
      task->cl_arg_size = sizeof(args);
      task->handles[0] = get_handle(A);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "assign_task");
    } else {
      assign_cpu_func(&A, A.dim[0], A.dim[1], A.stride, args);
    }
  }
};

void add_assign_task(Dense& A, double value) {
  add_task(std::make_shared<Assign_task>(A, value));
}

void addition_cpu_func(
  double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  const double* B, uint64_t B_stride
) {
  for (uint64_t i=0; i<A_dim0; i++) {
    for (uint64_t j=0; j<A_dim1; j++) {
      A[i*A_stride+j] += B[i*B_stride+j];
    }
  }
}

void addition_cpu_starpu_interface(void* buffers[], void*) {
  double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  const double* B = (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t B_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  addition_cpu_func(A, A_dim0, A_dim1, A_stride, B, B_stride);
}

struct starpu_codelet addition_cl;


class Addition_task : public Task {
 public:
  Addition_task(Dense& A, const Dense& B) : Task({B}, {A}) {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &addition_cl;
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(B);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    const Dense& B = constant[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "addition_task");
    } else {
      addition_cpu_func(&A, A.dim[0], A.dim[1], A.stride, &B, B.stride);
    }
  }
};

void add_addition_task(Dense& A, const Dense& B) {
  if (!matrix_is_tracked("addition_task", A, B) || !is_tracking) {
    add_task(std::make_shared<Addition_task>(A, B));
    register_matrix("addition_task", A, B);
  }
}

void subtraction_cpu_func(
  double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  const double* B, uint64_t B_stride
) {
  for (uint64_t i=0; i<A_dim0; i++) {
    for (uint64_t j=0; j<A_dim1; j++) {
      A[i*A_stride+j] -= B[i*B_stride+j];
    }
  }
}

void subtraction_cpu_starpu_interface(void* buffers[], void*) {
  double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  const double* B = (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t B_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  subtraction_cpu_func(A, A_dim0, A_dim1, A_stride, B, B_stride);
}

struct starpu_codelet subtraction_cl;


class Subtraction_task : public Task {
 public:
  Subtraction_task(Dense& A, const Dense& B) : Task({B}, {A}) {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &subtraction_cl;
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(B);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    const Dense& B = constant[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "subtraction_task");
    } else {
      subtraction_cpu_func(&A, A.dim[0], A.dim[1], A.stride, &B, B.stride);
    }
  }
};

void add_subtraction_task(Dense& A, const Dense& B) {
  add_task(std::make_shared<Subtraction_task>(A, B));
}

struct multiplication_args { double factor; };

void multiplication_cpu_func(
  double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  multiplication_args& args
) {
  for (uint64_t i=0; i<A_dim0; i++) {
    for (uint64_t j=0; j<A_dim1; j++) {
      A[i*A_stride+j] *= args.factor;
    }
  }
}

void multiplication_cpu_starpu_interface(void* buffers[], void* cl_args) {
  double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  struct multiplication_args* args = (multiplication_args*)cl_args;
  multiplication_cpu_func(A, A_dim0, A_dim1, A_stride, *args);
}

struct starpu_codelet multiplication_cl;


class Multiplication_task : public Task {
 public:
  multiplication_args args;
  Multiplication_task(Dense& A, double factor) : Task({}, {A}), args{factor} {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &multiplication_cl;
      task->cl_arg = &args;
      task->cl_arg_size = sizeof(args);
      task->handles[0] = get_handle(A);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "multiplication_task");
    } else {
      multiplication_cpu_func(&A, A.dim[0], A.dim[1], A.stride, args);
    }
  }
};

void add_multiplication_task(Dense& A, double factor) {
  // Don't do anything if factor == 1
  if (factor != 1) {
    add_task(std::make_shared<Multiplication_task>(A, factor));
  }
}

void getrf_cpu_func(
  double* AU, uint64_t AU_dim0, uint64_t AU_dim1, uint64_t AU_stride,
  double* L, uint64_t L_stride
) {
  std::vector<int> ipiv(std::min(AU_dim0, AU_dim1));
  LAPACKE_dgetrf(
    LAPACK_ROW_MAJOR,
    AU_dim0, AU_dim1,
    AU, AU_stride,
    &ipiv[0]
  );
  for (uint64_t i=0; i<AU_dim0; i++) {
    for (uint64_t j=0; j<i; j++) {
      L[i*L_stride+j] = AU[i*AU_stride+j];
      AU[i*AU_stride+j] = 0;
    }
    L[i*L_stride+i] = 1;
  }
}

void getrf_cpu_starpu_interface(void* buffers[], void*) {
  double* AU = (double*)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t AU_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t AU_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t AU_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  double* L = (double*)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t L_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  getrf_cpu_func(AU, AU_dim0, AU_dim1, AU_stride, L, L_stride);
}

struct starpu_codelet getrf_cl;


class GETRF_task : public Task {
 public:
  GETRF_task(Dense& AU, Dense& L) : Task({}, {AU, L}) {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &getrf_cl;
      task->handles[0] = get_handle(AU);
      task->handles[1] = get_handle(L);
    }
  }

  void submit() override {
    Dense& AU = modified[0];
    Dense& L = modified[1];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "getrf_task");
    } else {
      getrf_cpu_func(&AU, AU.dim[0], AU.dim[1], AU.stride, &L, L.stride);
    }
  }
};

void add_getrf_task(Dense& AU, Dense& L) {
  add_task(std::make_shared<GETRF_task>(AU, L));
}

void qr_cpu_func(
  double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  double* Q, uint64_t Q_dim0, uint64_t Q_dim1, uint64_t Q_stride,
  double* R, uint64_t, uint64_t, uint64_t R_stride
) {
  uint64_t k = std::min(A_dim0, A_dim1);
  std::vector<double> tau(k);
  for (uint64_t i=0; i<std::min(Q_dim0, Q_dim1); i++) Q[i*Q_stride+i] = 1.0;
  LAPACKE_dgeqrf(LAPACK_ROW_MAJOR, A_dim0, A_dim1, A, A_stride, &tau[0]);
  // TODO Consider using A for the dorgqr and moving to Q afterwards! That
  // also simplify this loop.
  for(uint64_t i=0; i<Q_dim0; i++) {
    for(uint64_t j=i; j<Q_dim1; j++) {
      R[i*R_stride+j] = A[i*A_stride+j];
    }
  }
  for(uint64_t i=0; i<Q_dim0; i++) {
    for(uint64_t j=0; j<std::min(i, Q_dim1); j++) {
      Q[i*Q_stride+j] = A[i*A_stride+j];
    }
  }
  // TODO Consider making special function for this. Performance heavy
  // and not always needed. If Q should be applied to something, use directly!
  // Alternatively, create Dense derivative that remains in elementary
  // reflector form, uses dormqr instead of gemm and can be transformed to
  // Dense via dorgqr!
  LAPACKE_dorgqr(
    LAPACK_ROW_MAJOR, Q_dim0, Q_dim1, k, Q, Q_stride, &tau[0]
  );
}

void qr_cpu_starpu_interface(void* buffers[], void*) {
  double* A = (double*)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  double* Q = (double*)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t Q_dim0 = STARPU_MATRIX_GET_NY(buffers[1]);
  uint64_t Q_dim1 = STARPU_MATRIX_GET_NX(buffers[1]);
  uint64_t Q_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  double* R = (double*)STARPU_MATRIX_GET_PTR(buffers[2]);
  uint64_t R_dim0 = STARPU_MATRIX_GET_NY(buffers[2]);
  uint64_t R_dim1 = STARPU_MATRIX_GET_NX(buffers[2]);
  uint64_t R_stride = STARPU_MATRIX_GET_LD(buffers[2]);
  qr_cpu_func(
    A, A_dim0, A_dim1, A_stride,
    Q, Q_dim0, Q_dim1, Q_stride,
    R, R_dim0, R_dim1, R_stride
  );
}

struct starpu_codelet qr_cl;


class QR_task : public Task {
 public:
  QR_task(Dense& A, Dense& Q, Dense& R) : Task({}, {A, Q, R}) {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &qr_cl;
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(Q);
      task->handles[2] = get_handle(R);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    Dense& Q = modified[1];
    Dense& R = modified[2];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "qr_task");
    } else {
      qr_cpu_func(
        &A, A.dim[0], A.dim[1], A.stride,
        &Q, Q.dim[0], Q.dim[1], Q.stride,
        &R, R.dim[0], R.dim[1], R.stride
      );
    }
  }
};

void add_qr_task(Dense& A, Dense& Q, Dense& R) {
  // TODO Check for duplicate/shared tasks
  add_task(std::make_shared<QR_task>(A, Q, R));
}

void rq_cpu_func(
  double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  double* R, uint64_t R_dim0, uint64_t R_dim1, uint64_t R_stride,
  double* Q, uint64_t Q_dim0, uint64_t Q_dim1, uint64_t Q_stride
) {
  uint64_t k = std::min(A_dim0, A_dim1);
  std::vector<double> tau(k);
  LAPACKE_dgerqf(LAPACK_ROW_MAJOR, A_dim0, A_dim1, A, A_stride, &tau[0]);
  // TODO Consider making special function for this. Performance heavy and not
  // always needed. If Q should be applied to something, use directly!
  // Alternatively, create Dense derivative that remains in elementary reflector
  // form, uses dormqr instead of gemm and can be transformed to Dense via
  // dorgqr!
  for (uint64_t i=0; i<R_dim0; i++) {
    for (uint64_t j=i; j<R_dim1; j++) {
      R[i*R_stride+j] = A[i*A_stride+A_dim1-R_dim1+j];
    }
  }
  for(uint64_t i=0; i<Q_dim0; i++) {
    for(uint64_t j=0; j<std::min(A_dim1-R_dim1+i, Q_dim1); j++) {
      Q[i*Q_stride+j] = A[i*A_stride+j];
    }
  }
  LAPACKE_dorgrq(
    LAPACK_ROW_MAJOR, Q_dim0, Q_dim1, k, Q, Q_stride, &tau[0]
  );
}

void rq_cpu_starpu_interface(void* buffers[], void*) {
  double* A = (double*)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  double* R = (double*)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t R_dim0 = STARPU_MATRIX_GET_NY(buffers[1]);
  uint64_t R_dim1 = STARPU_MATRIX_GET_NX(buffers[1]);
  uint64_t R_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  double* Q = (double*)STARPU_MATRIX_GET_PTR(buffers[2]);
  uint64_t Q_dim0 = STARPU_MATRIX_GET_NY(buffers[2]);
  uint64_t Q_dim1 = STARPU_MATRIX_GET_NX(buffers[2]);
  uint64_t Q_stride = STARPU_MATRIX_GET_LD(buffers[2]);
  rq_cpu_func(
    A, A_dim0, A_dim1, A_stride,
    R, R_dim0, R_dim1, R_stride,
    Q, Q_dim0, Q_dim1, Q_stride
  );
}

struct starpu_codelet rq_cl;


class RQ_task : public Task {
 public:
  RQ_task(Dense& A, Dense& R, Dense& Q) : Task({}, {A, R, Q}) {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &rq_cl;
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(R);
      task->handles[2] = get_handle(Q);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    Dense& R = modified[1];
    Dense& Q = modified[2];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "rq_task");
    } else {
      rq_cpu_func(
        &A, A.dim[0], A.dim[1], A.stride,
        &R, R.dim[0], R.dim[1], R.stride,
        &Q, Q.dim[0], Q.dim[1], Q.stride
      );
    }
  }
};

void add_rq_task(Dense& A, Dense& R, Dense& Q) {
  // TODO Check for duplicate/shared tasks
  add_task(std::make_shared<RQ_task>(A, R, Q));
}

struct trsm_args { int uplo; int lr; };

void trsm_cpu_func(
  const double* A, uint64_t A_stride,
  double* B, uint64_t B_dim0, uint64_t B_dim1, uint64_t B_stride,
  trsm_args& args
) {
  cblas_dtrsm(
    CblasRowMajor,
    args.lr==TRSM_LEFT?CblasLeft:CblasRight,
    args.uplo==TRSM_UPPER?CblasUpper:CblasLower,
    CblasNoTrans,
    args.uplo==TRSM_UPPER?CblasNonUnit:CblasUnit,
    B_dim0, B_dim1,
    1,
    A, A_stride,
    B, B_stride
  );
}

void trsm_cpu_starpu_interface(void* buffers[], void* cl_args) {
  const double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  double* B = (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t B_dim0 = STARPU_MATRIX_GET_NY(buffers[1]);
  uint64_t B_dim1 = STARPU_MATRIX_GET_NX(buffers[1]);
  uint64_t B_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  struct trsm_args* args = (trsm_args*)cl_args;
  trsm_cpu_func(A, A_stride, B, B_dim0, B_dim1, B_stride, *args);
}

struct starpu_codelet trsm_cl;


class TRSM_task : public Task {
 public:
  trsm_args args;
  TRSM_task(const Dense& A, Dense& B, int uplo, int lr)
  : Task({A}, {B}), args{uplo, lr} {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &trsm_cl;
      task->cl_arg = &args;
      task->cl_arg_size = sizeof(args);
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(B);
    }
  }

  void submit() override {
    const Dense& A = constant[0];
    Dense& B = modified[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "trsm_task");
    } else {
      trsm_cpu_func(&A, A.stride, &B, B.dim[0], B.dim[1], B.stride, args);
    }
  }
};

void add_trsm_task(const Dense& A, Dense& B, int uplo, int lr) {
  if (!matrix_is_tracked("trsm_task", A, B) || !is_tracking) {
    add_task(std::make_shared<TRSM_task>(A, B, uplo, lr));
    register_matrix("trsm_task", A, B);
  }
}

struct gemm_args { double alpha, beta; bool TransA, TransB;  };

void gemm_cpu_func(
  const double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  const double* B, uint64_t, uint64_t B_dim1, uint64_t B_stride,
  double* C, uint64_t C_dim0, uint64_t C_dim1, uint64_t C_stride,
  gemm_args& args
) {
  if (B_dim1 == 1) {
    cblas_dgemv(
      CblasRowMajor,
      CblasNoTrans,
      A_dim0, A_dim1,
      args.alpha,
      A, A_stride,
      B, B_stride,
      args.beta,
      C, C_stride
    );
  }
  else {
    int64_t k = args.TransA ? A_dim0 : A_dim1;
    cblas_dgemm(
      CblasRowMajor,
      args.TransA?CblasTrans:CblasNoTrans, args.TransB?CblasTrans:CblasNoTrans,
      C_dim0, C_dim1, k,
      args.alpha,
      A, A_stride,
      B, B_stride,
      args.beta,
      C, C_stride
    );
  }
}

void gemm_cpu_starpu_interface(void* buffers[], void* cl_args) {
  const double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  const double* B = (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t B_dim0 = STARPU_MATRIX_GET_NY(buffers[1]);
  uint64_t B_dim1 = STARPU_MATRIX_GET_NX(buffers[1]);
  uint64_t B_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  double* C = (double *)STARPU_MATRIX_GET_PTR(buffers[2]);
  uint64_t C_dim0 = STARPU_MATRIX_GET_NY(buffers[2]);
  uint64_t C_dim1 = STARPU_MATRIX_GET_NX(buffers[2]);
  uint64_t C_stride = STARPU_MATRIX_GET_LD(buffers[2]);
  struct gemm_args* args = (gemm_args*)cl_args;
  gemm_cpu_func(
    A, A_dim0, A_dim1, A_stride,
    B, B_dim0, B_dim1, B_stride,
    C, C_dim0, C_dim1, C_stride,
    *args
  );
}

struct starpu_codelet gemm_cl;

class GEMM_task : public Task {
 public:
  gemm_args args;
  GEMM_task(
    const Dense& A, const Dense& B, Dense& C,
    double alpha, double beta, bool TransA, bool TransB
  ) : Task({A, B}, {C}), args{alpha, beta, TransA, TransB} {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &gemm_cl;
      task->cl_arg = &args;
      task->cl_arg_size = sizeof(args);
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(B);
      task->handles[2] = get_handle(C);
      // Effectively write only, this might be important for dependencies
      if (args.beta == 0) STARPU_TASK_SET_MODE(task, STARPU_W, 2);
    }
  }

  void submit() override {
    const Dense& A = constant[0];
    const Dense& B = constant[1];
    Dense& C = modified[0];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "gemm_task");
    } else {
      gemm_cpu_func(
        &A, A.dim[0], A.dim[1], A.stride,
        &B, B.dim[0], B.dim[1], B.stride,
        &C, C.dim[0], C.dim[1], C.stride,
        args
      );
    }
  }
};

BasisTracker<
  uint64_t, BasisTracker<uint64_t, std::shared_ptr<GEMM_task>>
> gemm_tracker;

void add_gemm_task(
  const Dense& A, const Dense& B, Dense& C,
  double alpha, double beta, bool TransA, bool TransB
) {
  if (
    is_tracking
    && !C.is_submatrix()
    && gemm_tracker.has_key(A.id()) && gemm_tracker[A.id()].has_key(B.id())
    && gemm_tracker[A.id()][B.id()]->args.alpha == alpha
    && gemm_tracker[A.id()][B.id()]->args.beta == beta
    && gemm_tracker[A.id()][B.id()]->args.TransA == TransA
    && gemm_tracker[A.id()][B.id()]->args.TransB == TransB
  ) {
    if (C.id() == gemm_tracker[A.id()][B.id()]->modified[0].id()) {
      return;
    } else
    if (beta == 0) {
      C = gemm_tracker[A.id()][B.id()]->modified[0].shallow_copy();
      return;
    }
  }
  std::shared_ptr<GEMM_task> task = std::make_shared<GEMM_task>(
    A, B, C, alpha, beta, TransA, TransB
  );
  if (is_tracking && !C.is_submatrix()) {
    gemm_tracker[A.id()][B.id()] = task;
  }
  add_task(task);
}

void svd_cpu_func(
  double* A, uint64_t A_dim0, uint64_t A_dim1, uint64_t A_stride,
  double* U, uint64_t, uint64_t, uint64_t U_stride,
  double* S, uint64_t S_dim0, uint64_t, uint64_t S_stride,
  double* V, uint64_t, uint64_t, uint64_t V_stride
) {
  std::vector<double> Sdiag(S_dim0, 0);
  std::vector<double> work(S_dim0-1, 0);
  LAPACKE_dgesvd(
    LAPACK_ROW_MAJOR,
    'S', 'S',
    A_dim0, A_dim1,
    A, A_stride,
    &Sdiag[0],
    U, U_stride,
    V, V_stride,
    &work[0]
  );
  for(uint64_t i=0; i<S_dim0; i++){
    S[i*S_stride+i] = Sdiag[i];
  }
}

void svd_cpu_starpu_interface(void* buffers[], void*) {
  double* A = (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
  uint64_t A_dim0 = STARPU_MATRIX_GET_NY(buffers[0]);
  uint64_t A_dim1 = STARPU_MATRIX_GET_NX(buffers[0]);
  uint64_t A_stride = STARPU_MATRIX_GET_LD(buffers[0]);
  double* U = (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
  uint64_t U_dim0 = STARPU_MATRIX_GET_NY(buffers[1]);
  uint64_t U_dim1 = STARPU_MATRIX_GET_NX(buffers[1]);
  uint64_t U_stride = STARPU_MATRIX_GET_LD(buffers[1]);
  double* S = (double *)STARPU_MATRIX_GET_PTR(buffers[2]);
  uint64_t S_dim0 = STARPU_MATRIX_GET_NY(buffers[2]);
  uint64_t S_dim1 = STARPU_MATRIX_GET_NX(buffers[2]);
  uint64_t S_stride = STARPU_MATRIX_GET_LD(buffers[2]);
  double* V = (double *)STARPU_MATRIX_GET_PTR(buffers[3]);
  uint64_t V_dim0 = STARPU_MATRIX_GET_NY(buffers[3]);
  uint64_t V_dim1 = STARPU_MATRIX_GET_NX(buffers[3]);
  uint64_t V_stride = STARPU_MATRIX_GET_LD(buffers[3]);
  svd_cpu_func(
    A, A_dim0, A_dim1, A_stride,
    U, U_dim0, U_dim1, U_stride,
    S, S_dim0, S_dim1, S_stride,
    V, V_dim0, V_dim1, V_stride
  );
}

struct starpu_codelet svd_cl;


class SVD_task : public Task {
 public:
  SVD_task(Dense& A, Dense& U, Dense& S, Dense& V) : Task({}, {A, U, S, V}) {
    if (schedule_started) {
      task = starpu_task_create();
      task->cl = &svd_cl;
      task->handles[0] = get_handle(A);
      task->handles[1] = get_handle(U);
      task->handles[2] = get_handle(S);
      task->handles[3] = get_handle(V);
    }
  }

  void submit() override {
    Dense& A = modified[0];
    Dense& U = modified[1];
    Dense& S = modified[2];
    Dense& V = modified[3];
    if (schedule_started) {
      STARPU_CHECK_RETURN_VALUE(starpu_task_submit(task), "svd_task");
    } else {
      svd_cpu_func(
        &A, A.dim[0], A.dim[1], A.stride,
        &U, U.dim[0], U.dim[1], U.stride,
        &S, S.dim[0], S.dim[1], S.stride,
        &V, V.dim[0], V.dim[1], V.stride
      );
    }
  }
};

void add_svd_task(Dense& A, Dense& U, Dense& S, Dense& V) {
  add_task(std::make_shared<SVD_task>(A, U, S, V));
}

void start_schedule() {
  assert(!schedule_started);
  assert(tasks.empty());
  schedule_started = true;
}

void execute_schedule() {
  for (std::shared_ptr<Task> task : tasks) {
    task->submit();
  }
  starpu_task_wait_for_all();
  tasks.clear();
  schedule_started = false;
}

void initialize_starpu() {
  STARPU_CHECK_RETURN_VALUE(starpu_init(NULL), "init");
  //make_copy_codelet();
  //make_transpose_codelet();
  //make_assign_codelet();
  //make_addition_codelet();
  //make_subtraction_codelet();
  //make_multiplication_codelet();
  //make_getrf_codelet();
  //make_qr_codelet();
  //make_rq_codelet();
  //make_trsm_codelet();
  //make_gemm_codelet();
  //make_svd_codelet();
}

void start_tracking() {
  assert(!is_tracking);
  is_tracking = true;
}

void stop_tracking() {
  assert(is_tracking);
  is_tracking = false;
}

} // namespace hicma

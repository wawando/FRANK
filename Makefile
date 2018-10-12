.SUFFIXES: .cpp .cu

CXX = mpicxx -std=c++11 -ggdb3 -Wall -O3 -fopenmp -I. -Wfatal-errors
NVCC = nvcc -ccbin=g++-5 -std=c++11 -I.

SOURCES = errors.o print.o mpi_utils.o timer.o functions.o dense.o low_rank.o hierarchical.o node.o any.o
TEST_SOURCES = test/test_helper.o test/test_mpi_block_creation.o test/test_mpi_dense_lu.o

.cpp.o:
	$(CXX) -c $? -o $@

.cu.o:
	$(NVCC) -c $? -o $@

rsvd: rsvd.o $(SOURCES)
	$(CXX) $? -lblas -llapacke
	valgrind ./a.out

gpu: gpu.o $(SOURCES)
	$(CXX) $? -lblas -llapacke -lcudart
	valgrind ./a.out

block_lu: block_lu.o $(SOURCES)
	$(CXX) $? -lblas -llapacke
	valgrind ./a.out

blr_lu: blr_lu.o $(SOURCES)
	$(CXX) $? -lblas -llapacke
	valgrind ./a.out

h_lu: h_lu.o $(SOURCES)
	$(CXX) $? -lblas -llapacke
	valgrind ./a.out 6

test:  $(TEST_SOURCES) $(SOURCES)
	$(CXX) $? -lblas -llapacke
	mpirun -np 4 ./a.out

clean:
	$(RM) *.o *.out *.xml

## getting echll-benchmark

The simplest way to get echll-benchmark is to download echll and
echll-benchmark, install dependencies, compile and install:

For recent debian and ubuntu derivatives:

    apt-get install build-essential cmake g++ clang libboost-dev \
            libboost-serialization-dev libboost-mpi-dev libopenmpi-dev \
	    openmpi-bin

    git clone https://github.com/quesnel/Echll.git
    cd Echll
    mkdir build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release ..
    make -j8
    sudo make install

    git clone https://github.com/quesnel/Echll-Benchmark.git
    cd Echll-Benchmark
    mkdir build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release ..
    make -j8
    sudo make install

To use clang replace the previous `cmake` command:

    CXX=clang++-libc++ cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release ..

## examples

Echll-benchmark is an executable. It takes several parameters. Use the help
option `-h`.

    cd examples

    # lauch the benchmark with 200ms as timer and all available threads.
    Echll-benchmark -t 3 -d 200 ROOT.tgf
    
    # launch the benchmark with 3 processors using MPI
    mpirun -np 3 Echll-benchmark -t 0 -d 200 ROOT.tgf

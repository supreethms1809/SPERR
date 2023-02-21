#
# The line below states we will base our new image on the Latest Official Ubuntu 
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04
#
# Identify the maintainer of an image
LABEL maintainer="shaomeng@ucar.edu"
LABEL maintainer="ssuresh@ucar.edu"
#
# Update the image to the latest packages
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y git vim libzstd-dev pkg-config libgtest-dev cmake-curses-gui build-essential && \
    groupadd -r Odie && useradd -r -g Odie Odie && \
    mkdir -p /home/Odie && cd /home && chown Odie:Odie Odie
#
# Switch user
USER Odie
#

#set CUDA
ENV CUDA /usr/local/cuda
ENV NVCC /usr/local/cuda/bin/nvcc
ENV LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/cuda/lib64
ENV LD_RUN_PATH=${LD_RUN_PATH}:/usr/local/cuda/lib64
ENV PATH=${PATH}:/usr/local/cuda/bin
ENV CC=gcc
ENV CXX=g++

# Clone the kokkos repo, and build it
RUN cd /home/Odie && \
    git clone https://github.com/kokkos/kokkos.git && \
    cd kokkos && \
    mkdir -p build install && \
    cd build && \
    cmake -DBUILD_SHARED_LIBS=ON  \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_STANDARD=17 \
	-DKokkos_ARCH_VOLTA70=ON   \
	-DKokkos_ARCH_SKX=ON       \
	-DKokkos_ENABLE_CUDA=ON    \
	-DKokkos_CUDA_DIR=/usr/local/cuda/  \
	-DKokkos_ENABLE_CUDA_LAMBDA=ON    \
	-DKokkos_ENABLE_CUDA_UVM=ON    \
	-DKokkos_ENABLE_OPENMP=ON   \
	-DKokkos_ENABLE_SERIAL=ON   \
	-DCMAKE_CXX_COMPILER=/home/Odie/kokkos/bin/nvcc_wrapper \
	-DCMAKE_C_COMPILER=/home/Odie/kokkos/bin/nvcc_wrapper  \
	-DCMAKE_CXX_FLAGS=--expt-relaxed-constexpr   \
	-DCMAKE_C_FLAGS=--expt-relaxed-constexpr    \
	-DCMAKE_INSTALL_PREFIX=/home/Odie/kokkos/install/ .. && \
    make -j4 && make install

ENV KOKKOS_INSTALL=/home/Odie/kokkos/install/
ENV Kokkos_ROOT=${KOKKOS_INSTALL}
ENV CC=gcc
ENV CXX=${KOKKOS_INSTALL}/bin/nvcc_wrapper

# Clone the repo, and build it
RUN cd /home/Odie && \
    git clone --depth 1 --branch kokkos_ext_lib https://github.com/supreethms1809/SPERR.git SPERR_src
#
# Update PATH enviornment variable
ENV PATH="$PATH:/home/Odie/SPERR-src/build/bin"

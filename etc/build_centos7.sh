# Install dependencies for build on centos7

# Install dev and runtime dependencies
yum group install -y "Development Tools" \
    && yum install -y https://repo.ius.io/ius-release-el7.rpm \
    && yum install -y centos-release-scl \
    && yum install -y wget devtoolset-8 \
    devtoolset-8-libatomic-devel tcl-devel tcl tk libstdc++ tk-devel pcre-devel \
    python36u python36u-libs python36u-devel python36u-pip && \
    yum clean -y all && \
    rm -rf /var/lib/apt/lists/*

# Install CMake
wget https://cmake.org/files/v3.14/cmake-3.14.0-Linux-x86_64.sh && \
    chmod +x cmake-3.14.0-Linux-x86_64.sh  && \
    ./cmake-3.14.0-Linux-x86_64.sh --skip-license --prefix=/usr/local && rm -rf cmake-3.14.0-Linux-x86_64.sh \
    && yum clean -y all

# Install epel repo
wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm && \
    yum install -y epel-release-latest-7.noarch.rpm && rm -rf epel-release-latest-7.noarch.rpm  \
    && yum clean -y all

# Install any git version > 2.6.5
yum remove -y git* && yum install -y git224

# Install SWIG
yum remove -y swig \
    && wget https://github.com/swig/swig/archive/rel-4.0.1.tar.gz \
    && tar xfz rel-4.0.1.tar.gz \
    && rm -rf rel-4.0.1.tar.gz \
    && cd swig-rel-4.0.1 \
    && ./autogen.sh && ./configure --prefix=/usr && make -j $(nproc) && make install \
    && cd .. \
    && rm -rf swig-rel-4.0.1

# boost 1.68 required for TritonRoute but 1.68 unavailable
wget https://sourceforge.net/projects/boost/files/boost/1.72.0/boost_1_72_0.tar.bz2/download && \
    tar -xf download && \
    cd boost_1_72_0 && \
    ./bootstrap.sh && \
    ./b2 install --with-iostreams -j $(nproc)

# eigen required by replace, TritonMacroPlace
git clone https://gitlab.com/libeigen/eigen.git \
    && cd eigen \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make install

# lemon required by TritonCTS (no package for CentOS!)
#  (On Ubuntu liblemon-dev can be used instead)
wget http://lemon.cs.elte.hu/pub/sources/lemon-1.3.1.tar.gz \
    && tar -xf lemon-1.3.1.tar.gz \
    && cd lemon-1.3.1 \
    && cmake -B build . \
    && cmake --build build -j $(nproc) --target install

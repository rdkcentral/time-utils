WORKDIR=`pwd`
export ROOT=/usr
export INSTALL_DIR=${ROOT}/local
mkdir -p $INSTALL_DIR

# Build libchronyctl (shared library + test_timectl CLI)
cd $WORKDIR
export INSTALL_DIR='/usr/local'
export top_srcdir=`pwd`
export top_builddir=`pwd`
autoreconf -fiv
./configure --prefix=${INSTALL_DIR}
make && make install

WORKDIR=`pwd`

cd libchronyctl
# Build libchronyctl (shared library + test_timectl CLI)
export INSTALL_DIR='/usr/local'
autoreconf -i
./configure --prefix=${INSTALL_DIR}
make && make install

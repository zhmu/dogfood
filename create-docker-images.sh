#!/bin/sh

docker 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Docker not found; please install and re-run"
    exit 1
fi

set +e
. ./settings.sh

BUILD_DIR=`mktemp -d`
trap "rm -rf ${BUILD_DIR}" EXIT
echo "Preparing build directory; ${BUILD_DIR}"
cp ci/toolchain/Dockerfile ${BUILD_DIR}
mkdir -p ${BUILD_DIR}/work/userland
cp -r "userland/binutils-${BINUTILS_VERSION}" ${BUILD_DIR}/work/userland
cp -r "userland/gcc-9.2.0" ${BUILD_DIR}/work/userland
cp -r settings.sh ${BUILD_DIR}/work
cp -r build-toolchain.sh ${BUILD_DIR}/work

docker build -t dogfood-toolchain:latest -f ${BUILD_DIR}/Dockerfile ${BUILD_DIR}
docker build -t dogfood-buildimage:latest ci/build-image

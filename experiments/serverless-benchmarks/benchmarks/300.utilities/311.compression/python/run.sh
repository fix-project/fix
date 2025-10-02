#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
FIXPATH="${REALPATH}/../../../../../../"

cd ${REALPATH}
./setup.sh

pushd ${FIXPATH}
cmake --build build/applications-prefix/src/applications-build/ --target python_fixpoint
popd

${FIXPATH}/build/src/tests/run-python ${FIXPATH}/build/applications-prefix/src/applications-build/flatware/examples/python/python-fixpoint.wasm . "python workingdir/function.py acmart-master"

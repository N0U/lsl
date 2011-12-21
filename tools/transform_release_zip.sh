#!/usr/bin/env bash

#set -e
cd $(dirname $0)/..

#assumes absolute path
ZIPFILE=${1}
STRIP=${2}
if [ ! -w ${ZIPFILE} ]; then
	echo "cannot write to ${ZIPFILE}"
	exit 1
fi

if [ ! -x ${STRIP} ]; then
	echo "cannot execute strip binary at ${STRIP}"
	exit 1
fi

TMPDIR=/tmp/slzip
mkdir -p ${TMPDIR}
cd ${TMPDIR}
rm -rf libSpringLobby*
#just the filename
ZIPFILEBASE=$(echo $(basename $1))
#remove .zip postfix to get the dirname inside the zip
TOPLEVELDIR=${ZIPFILEBASE%".zip"}
unzip ${ZIPFILE}
rm ${ZIPFILE}
cd ${TOPLEVELDIR} && ${STRIP} boost*.dll wx*.dll mingwm10.dll && zip -r ${ZIPFILE} * locale/\* || exit 1
exit 0

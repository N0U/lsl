#! /usr/bin/env bash

set -e
cd $(dirname $0)/../$1
pwd

if [ x$2 == x ]; then
	filename=sl_master.zip
else
	filename=sl_${2}.zip
fi

zip -9 -u ${filename} libSpringLobby.exe

if [ ! -d /usr/local/www/libSpringLobby.info/temp/builds/$DEVELOPER ] ; then
	mkdir -p /usr/local/www/libSpringLobby.info/temp/builds/$DEVELOPER
fi

/usr/bin/install -m 0755 ${filename} /usr/local/www/libSpringLobby.info/temp/builds/$DEVELOPER/${filename}

echo "http://libSpringLobby.info/temp/builds/$DEVELOPER/${filename}"

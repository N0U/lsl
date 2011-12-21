#! /usr/bin/env bash

set -e
cd $(dirname $0)/../${1}
pwd
DEVELOPER=${2}
/opt/mingw32/bin/i586-pc-mingw32-strip libSpringLobby.exe

if [ x$3 == x ]; then
	filename=sl_master.zip
else
	filename=sl_${3}.zip
fi

zip -9 -u ${filename} libSpringLobby.exe

if [ ! -d /usr/local/www/libSpringLobby.info/temp/builds/$DEVELOPER ] ; then
	mkdir -p /usr/local/www/libSpringLobby.info/temp/builds/$DEVELOPER
fi

/usr/bin/install -m 0755 ${filename} /usr/local/www/libSpringLobby.info/temp/builds/$DEVELOPER/${filename}

echo "http://libSpringLobby.info/temp/builds/$DEVELOPER/${filename}"

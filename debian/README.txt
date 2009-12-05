Always build with:
CFLAGS_APPEND="-Isrc" LDFLAGS_APPEND="-Lsrc -lvalhalla" dpkg-buildpackage -rfakeroot -b -us -uc -nc -tc

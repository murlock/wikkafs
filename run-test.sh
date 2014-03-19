#!/bin/sh

do_run() {
	sh -c "$1"	
	return $?
}

run() {
	echo -n "$1    : "
	do_run "$2" 2>/dev/null >/dev/null
	if [ $? -ne 0 ]; then
		echo "Failed : $2"
	else
		echo "Pass"
	fi
}

if [ ! -d ./test ]; then
	mkdir test
fi

run "Mount" "./wikkafs test"

run "Reading" "cat test/MaPage >/tmp/MaPage.$$"

run "concat" "echo -e "toto" >> test/MaPage"

run "rewrite" "cat test/MaPage | sed s~toto~titi~g > test/MaPage"

run "overwrite page" "mv /tmp/MaPage.$$ test/MaPage"

run "Unmount" "fusermount -u test"

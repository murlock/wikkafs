wikkafs
=======

Provide a FUSE view of a wikawiki, useful to fill wiki page from script shell


usage
=====

```
WikkaFS: 
usage: wikkafs [OPTS] mountpoint
  -d : fuse debug (message on stderr)
  -f : fuse foreground (message on stderr)
  -w : wiki user (should be set otherwise updated/created will be as orphans !)
  -t : mysql prefix for table
  -u : sql username (default: wiki)
  -p : sql password (default: wiki)
  -b : sql database to use (default: wiki)
  -h : host (default: 127.0.0.1)
  -l : port to use (default: 3306)
  -s : use SSL with connect to SQL

You can also use WIKKAFS as env variable
```

example
=======

```
# mount
$ ./wikkafs -w murlock -t wikka -u wik$iadmin -b wikiadmin -p mypassword /mnt/wiki/
# list pages
$ ls /mnt/wiki/Linux*
/mnt/wiki/Linux            /mnt/wiki/LinuxFS    /mnt/wiki/LinuxOcr        /mnt/wiki/LinuxSsh      /mnt/wiki/LinuxVServer
/mnt/wiki/LinuxContainers  /mnt/wiki/LinuxGrub  /mnt/wiki/LinuxPackaging  /mnt/wiki/LinuxSynergy  /mnt/wiki/LinuxWiimote
/mnt/wiki/LinuxCryptedFS   /mnt/wiki/LinuxIPV6  /mnt/wiki/LinuxShell      /mnt/wiki/LinuxVpn

# create a new page (must use camel format)
$ echo "This is draft webpage" > /mnt/wiki/DraftWebpage
```


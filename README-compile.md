
# Compile and install from source

* Install gcc >= 5.0. Other compilers might work but are untested.
* Install cmake >= 3.6 and make
* For safe generation of uuids it is recommend to install uuidd (uuid-runtime)
* Install qt-dev, uuid-dev, qt-sqlite-driver, Qt version >= 5.6.
  *With a little effort, shournal could be modified to
  support Qt version >= 5.3. Please open an issue, if that would
  be helpful to you.*
* To build the kernel-module the headers are also required
  (linux-headers-$(uname -r))

  *Packages lists*:

  Debian:
  ~~~
  apt-get install g++ cmake make qtbase5-dev libqt5sql5-sqlite \
   uuid-dev libcap-dev uuid-runtime linux-headers-$(uname -r) dkms

  ~~~
  Ubuntu:
  ~~~
  apt-get install g++ cmake make qtbase5-dev libqt5sql5-sqlite \
   uuid-dev libcap-dev uuid-runtime linux-headers-generic dkms
  ~~~
  Ubuntu HWE:
  ~~~
  apt-get install g++ cmake make qtbase5-dev libqt5sql5-sqlite \
   uuid-dev libcap-dev uuid-runtime linux-headers-generic-hwe-$(lsb_release -rs) dkms
  ~~~
  Opensuse:
  ~~~
  zypper install gcc-c++ cmake make libqt5-qtbase-devel \
   libQt5Sql5-sqlite libuuid-devel libcap-devel uuidd \
   kernel-default-devel dkms
  ~~~
  Arch Linux:
  ~~~
  yay -S gcc cmake make qt5-base uuid libcap linux-headers dkms
  ~~~

  CentOS (note: CentOS 7 as of July 2019 only ships with gcc 4.8
  -> compile gcc >= 5.0 yourself. cmake3 and cmake are seperate packages
  where cmake in version 2 is the default. Please ensure to compile with
  cmake3. The kernel 3.10 is too old for *shournal*'s kernel-module.
  Either install a newer one or stick with the fanotify-edition):
  ~~~
  yum install gcc-c++ cmake3 make qt5-qtbase-devel libuuid-devel \
  libcap-devel uuidd kernel-devel dkms
  ~~~

* In the source-tree-directory, enter the following commands to
  compile and install. By default `SHOURNAL_EDITION` `full` is built (see below).
  Supported options include `full, docker, ko, fanotify`.
  The `ko` (kernel module) edition does not install the fanotify backend
  which may be desirable for security reasons as the setuid-binary
  `shournal-run-fanotify` is omitted. For a description of the other editions
  refer to [Binary releases](./README.md#binary-releases).
  ~~~
  mkdir -p build
  cd build
  # If you later want to generate a deb-package, it is recommended
  # to use /usr as prefix: -DCMAKE_INSTALL_PREFIX=/usr
  cmake -DSHOURNAL_EDITION=full ..
  make
  # as root:
  make install
  # or if using a Debian-based distribution, generate a .deb-package:
  cpack -G DEB
  ~~~


**After compile and install**: <br>
If you created a .deb-package, please see
[Binary releases](./README.md#binary-releases). **Otherwise:**

**Kernel-module backend** <br>
For a quick test, the module can be loaded right from the build-tree:
`$ insmod kernel/shournalk.ko`. <br>
To install the kernel-module (not built in SHOURNAL_EDITION's
*docker* and *fanotify*) it is recommended to install it using dkms, e.g.:
~~~
dkms build shournalk/2.4    # adjust version as needed.
dkms install shournalk/2.4
# and load it with
modprobe shournalk
~~~
Depending on your distribution the dkms service may be disabled, thus
after a kernel-update shournal stops working. At least on
Opensuse Tumbleweed it can be enabled with
~~~
systemctl enable dkms
~~~

**fanotify backend** <br>
Add a group to your system, which is primarily needed for
the shell-integration:

  ```groupadd shournalmsenter```

However, *do not add any users to that group*. It is part of a permission
check, where root adopts that gid (within shournal).
If you don't like the default group name, you can specify your own: at
build time pass the following to cmake:

  ```-DMSENTER_GROUPNAME=$your_group_name```

For **further post-install steps** please see
[Binary releases](./README.md#binary-releases). Please note
that file-paths may need to be adjusted, e.g. the location of
the `SOURCE_ME.$shell_name` scripts after `make install` is typically
`/usr/local/share/shournal/`, not `/usr/share/shournal/`.


To **uninstall**, after having installed with `make install`, you can
execute <br>
`xargs rm < install_manifest.txt`, but see
[here](https://stackoverflow.com/a/44649542/7015849) for the
limitations. <br>
To uninstall the kernel-module backend: <br>
`sudo dkms remove shournalk/2.4` (adjust version as needed).

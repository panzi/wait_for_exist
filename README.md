wait for exists
===============

[![Test Status](https://img.shields.io/github/actions/workflow/status/panzi/wait_for_exist/tests.yml)](https://github.com/panzi/wait_for_exist/actions/workflows/tests.yml)
[![GNU General Public License 3.0](https://img.shields.io/github/license/panzi/wait_for_exist)](https://github.com/panzi/wait_for_exist/blob/main/LICENSE.txt)

Simple C program to wait for a path to appear using inotify.

Just for fun.

Usage
-----

```
Usage: wait_for_exist: [--help] [--version] [--timeout=SECONDS] <path>

OPTIONS:

    -h, --help               Print this help message.
    -v, --version            Print version.
    -t, --timeout=SECONDS    Timeout after SECONDS without a new inotify event.
```

### Example

```bash
wait_for_exist /var/log/some.log
tail -f /var/log/some.log
```

Build
-----

### Parameters

```
BUILD_TYPE=release|debug
PREFIX=/usr/local
```

### Targets

- `all` - build the binary
- `clean` - delete build files
- `test` - run tests
- `build-test` - only build the test binary
- `valgrind` - run tests through [valgrind](https://valgrind.org/)
- `install` - copy the binary to `$PREFIX/bin/wait_for_exist`
- `uninstall` - delete `$PREFIX/bin/wait_for_exist`

### Example

```bash
make all
make test
make install
```

License
-------

[GPL v3](./LICENSE.txt)

# analytics-cpp

[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/segmentio/analytics-cpp/blob/master/LICENSE)
[![Linux Status](https://img.shields.io/travis/segementio/analytics-cpp/master.svg?label=linux)](https://travis-ci.org/segmentio/analytics-cpp)
[![Windows Status](https://img.shields.io/appveyor/ci/segmentio/analytics-cpp/master.svg?label=windows)](https://ci.appveyor.com/project/segmentio/analytics-cpp)
[![Test Coverage](https://coveralls.io/repos/segmentio/analytics-cpp/badge.svg?branch=master)](https://coveralls.io/r/segmentio/analytics-cpp)

Build status on Garrett's Branch:
---------------------------------

[![Linux/Mac Status](https://travis-ci.org/segmentio/analytics-cpp.svg?branch=garrett)](https://travis-ci.org/gdamore/analytics-cpp)
[![Windows Status](https://img.shields.io/appveyor/ci/gdamore/analytics-cpp/garrett.svg?label=windows)](https://ci.appveyor.com/project/gdamore/analytics-cpp)
[![Test Coverage](https://coveralls.io/repos/gdamore/analytics-cpp/badge.svg?branch=coverall)](https://coveralls.io/r/gdamore/analytics-cpp)

A C++ implementation for sending analytics events to Segment.
This requires C++11, which means GCC 4.9 or newer; Clang 3.4 or newer; and
Visual Studio 2015 or newer.

This library is in a beta state and its API/ABI may change.
**This is not production-ready software.**

## Building

You will need CMake 3.1 to build this.  Standard CMake recipes work, and a CTest
based test is included.

```
   $ mkdir build
   $ cd build
   $ cmake ..
   $ cmake --build .
```

On Windows, the default is to use the WinInet library for transport, which
is a standard system component supplied by Microsoft.

On other systems (Mac, Linux) you need to have libcurl installed.  Modern macOS
has a suitable version already installed.  Note that versions of libcurl built
against openssl do not work well with valgrind.  The GnuTLS version works well
-- on Ubuntu do `apt-get install libcurl4-gnutls-dev` for the goodness.

## Replacing the HTTP Client

You can elide the default HTTP client, and provide your own transport.
To eliminate the stock and use a stub instead, use `-DNO_DEFAULT_HTTP=ON` on
the CMake configure line.

You can also simply replace the existing handler by replacing the `Handler`
member of the `Analytics` class with your own implementation.

If you didn't elide the stock transport, you can still override it, and even
use the default `Handler` form within your own.  For example, you could use
a version that simply rewrites HTTP headers without being required to
implement the mechanics of the HTTP protocol itself.

## Examples

There is an example program, `example.c`.  The vanilla client API is
documented in the `analytics.hpp` header file.

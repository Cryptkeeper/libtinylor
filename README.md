# tinylor

A C99 single-file header library for controlling Light-O-Rama hardware in under 500 LOC. This is a more portable, but less powerful version of my [liblorproto](https://github.com/Cryptkeeper/liblorproto) library intended for use where you "just" want to control lights.

## Usage

`tinylor.h` follows the single-header pattern. To use, include `tinylor.h` in your project and define `TINYLOR_IMPL` exactly once in a C source file of your choice. [More information on using single-header libraries.](https://github.com/nothings/stb/tree/master?tab=readme-ov-file#how-do-i-use-these-libraries)

```c
#define TINYLOR_IMPL
#include "tinylor.h"
```

For specific usage details of the C API, see the pre-compiled [tinylor.h](tinylor.h) or visit the [Doxygen documentation](https://cryptkeeper.github.io/libtinylor).

## Building

1. `git clone https://github.com/Cryptkeeper/libtinylor`
2. `cd libtinylor`
3. `cmake -Bbuild`
4. `cmake --build build`

A static library artifact is available as `build/libtinylor.a`, and the generated single-header file is available as `build/tinylor.h`.

## Testing

Basic test coverage is provided by [tinylor_test.c](src/tinylor_test.c). If you have already built the project using CMake, you can execute the tests by entering the build directory and executing `ctest .`.

## Examples

Several usage examples are included in [examples.c](examples.c).

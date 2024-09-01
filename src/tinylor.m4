// tinylor.h - Single-file header library for controlling Light-O-Rama hardware
// https://github.com/Cryptkeeper/tinylor
// MIT licensed
// define TINYLOR_IMPL before including this file in a source file
#ifndef TINYLOR_SINGLEFILE_H
#define TINYLOR_SINGLEFILE_H
#define TINYLOR_IMPL_NO_INCLUDE
#line 1 "tinylor.h"
include(`src/tinylor.h')
#ifdef TINYLOR_IMPL
#ifndef TINYLOR_IMPL_ONCE
#define TINYLOR_IMPL_ONCE
#line 1 "tinylor.c"
include(`src/tinylor.c')
#endif// TINYLOR_IMPL_ONCE
#endif// TINYLOR_IMPL
#endif// TINYLOR_SINGLEFILE_H

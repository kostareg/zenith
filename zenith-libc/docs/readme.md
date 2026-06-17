# Zenith C Standard Library

This directory implements the Zenith C standard library. All C files in this directory are **Zenith C**. See
`zenith-compiler` for more details.

This library is largely inspired by the C99 standard library. However, due to restrictions in Zenith C's implementation
(e.g. no varargs) and Zenith kernel implementation (e.g. minimal system calls), we are unable to implement everything
in the same fashion. I would love to eventually implement a better compiler, kernel, and standard library, but for now,
this project is serving as a proof of concept and will be minimal.

Some documentation for this library is provided inline with the code (see `stdio.h`'s `printf`, `printf_ints`), and
it's all kept simple enough to understand by a quick read.
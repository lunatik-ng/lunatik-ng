lunatik-ng
==========
This work is part of the research project: [Towards Dynamic Scripted pNFS Layouts (pdf)](http://www.pdsw.org/pdsw12/papers/grawinkle-pdsw12.pdf)


This repository contains the ongoing effort of porting the [Lunatik Lua engine](http://sourceforge.net/projects/lunatik/) to current
Linux kernels. There are a few differences between the original lunatik and lunatik-ng:

* Lunatik-ng works on x86\_64
* It is memory-leak free
* Modifying tables with constructs such as `buf = { 123 }; buf = foo(buf)` does not cause the kernel to `BUG()`. A side effect of this
  is that, contrary to the original lunatik, lunatik-ng does not support asynchroneous execution of Lua code. This will be added back
  at a later time.
* A few interfaces to the kernel are provided by default:
  * `buffer` for allocating memory regions in kernel space
  * `crypto` which provides bindings to the SHA1 implementation in the kernel (a more advanced interface to the kernel which allows
    selection of the cipher is in the works) and the random number generator.
  * `printk` as a direct binding to the kernels `printk`
  * `type` and `gc_count` as bindings to parts of the default Lua library

Try it out
----------
* Grab the latest Linux kernel tarball from kernel.org (e.g. [from here](www.kernel.org/pub/linux/kernel/v3.0/linux-3.6.6.tar.bz2))
* Extract it to `some_folder`
* Copy the contents of this directory structure to `some_folder`: `cp -R this_folder some_folder`
* Build the kernel as usual, make sure to check `Library functions -> Lunatik Lua engine`
* The patches add syscall #350 to the kernel. The syscall has the following prototype: `SYS_LUA (const char *code, size_t code_sz,
  char *result, size_t result_sz)`
* To make sure lunatik is working, you can execute the following lua code via the syscall: `return type({ 123 })` which should place
  the string `number` in `result`.

The buffer library
------------------
Calling `buffer.new(x)` returns a new buffer object of size `x`. The buffer object represents a `char *` of length `x`. Modifying and
reading the buffer follows the familiar table interface:

    buf = buffer.new(16)
    buf[1]  = 100
    buf[16] = 255

Buffer objects are used and returned by some of the provided bindings, such as `crypto.sha1` and `crypto.random`.

The Crypto library
------------------
The crypto library contains two functions, `crypto.sha1` and `crypto.random`. `crypto.random(x)` returns a buffer object of size `x`
filled with random data. The function is an almost direct mapping to the kernel's `get_random_bytes` function.

`crypto.sha1(x)` returns a buffer of size 20 bytes (one SHA1 hash) and takes as its parameter `x` a buffer of data to be hashed. At
the moment, the size of `x` is limited to `PAGE_SZ`, which is 4Kb on most x86 machines.

lunatik-ng
==========

This repository contains the ongoing effort of porting the [http://sourceforge.net/projects/lunatik/](Lunatik Lua engine) to current
Linux kernels. There are a few differences between the original lunatik and lunatik-ng:

* Lunatik works on x86\_64
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

* Grab the latest Linux kernel tarball from kernel.org (e.g. [www.kernel.org/pub/linux/kernel/v3.0/linux-3.6.6.tar.bz2](from here))
* Extract it to `some_folder`
* Copy the contents of this directory structure to `some_folder`:

    cp -R this_folder some_folder

* Build the kernel as usual, make sure to check `Library functions -> Lunatik Lua engine`

* The patches add syscall #350 to the kernel. The syscall has the following prototype:

    SYS_LUA (const char *code, size_t code_sz, char *result, size_t result_sz)

  to make sure lunatik is working, you can execute the following lua code via the syscall:

    return type({ 123 })

  which should place the string `number` in `result`.

This repository contains a set of C++ classes that can be used to implement inline caches for the input and output global array arguments (i.e. those mapped to external DRAM) of accelerators implemented on FPGAs via high-level synthesis.

The caches, their usage and the corresponding results have been published in <insert citation when ready>.

Main sub-directories:
* cache: header files declaring and defining the cache classes and their operators and methods.
* bitonic_sort, lucas_kanade, matrix_mul, smith_wat: a set of applications (described in the paper above) that demostrate the use of the caches. Each directory contains:
  * the source code for the application,
  * three sub-directories with the Makefiles for sdaccel that enable the compilation, synthesis and simulation of each application with three different memory architectures (as discussed in the paper):
    * global_mem: arrays are directly mapped to global memory without cache (worst performance, best resource usage)
    * local_mem: arrays are mapped to on-chip memory (unrealistic, since on-chip memory is not large enough; best performance, worst resource usage)
    * cache_mem: arrays are mapped to global memory and use our caches (good performance, good area)

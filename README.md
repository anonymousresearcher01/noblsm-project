# NobLSM Project
This repository is a modified version of the Linux kernel with a custom LSM-tree KV-Store, called **NobLSM (DAC'22)** ([Paper](https://dl.acm.org/doi/abs/10.1145/3489517.3530470)).

# Overview

# Setup
- OS: Ubuntu 20.04 or 22.04
- Kernel: 6.8
- RocksDB: v7.
- Compiler: GCC / CMake



This project integrates:
- Linux kernel (GPL-2.0 WITH Linux-syscall-note): https://github.com/anonymousresearcher01/linux/tree/nob
- RocksDB (Apache-2.0): Link

License:
- This project is released under the GNU General Public License v2 (GPLv2).
- Linux kernel retains its GPL-2.0 WITH Linux-syscall-note license.
- RocksDB retains its Apache-2.0 license.

# Reference
- RocksDB Wiki: https://github.com/facebook/rocksdb/wiki
- Linux kernel reference: https://elixir.bootlin.com
- JBD2: [linux/Documentation/filesystems/ext4/](https://github.com/torvalds/linux/tree/master/Documentation/filesystems/ext4)
- NobLSM (DAC '22) Paper: https://dl.acm.org/doi/abs/10.1145/3489517.3530470
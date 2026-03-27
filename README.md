# NobLSM Project
This repository is a modified version of the Linux kernel with a custom LSM-tree KV-Store, called **NobLSM (DAC'22)** ([Paper](https://dl.acm.org/doi/abs/10.1145/3489517.3530470)).

# Overview

Key-Value Store (KV-Store) periodically triggers major compaction.
The major compation generally includes (i) reading SSTable from disk, (2) merging into a new SSTable, (3) writing back to disk.
During the major compaction, KV-Store relying on underlying filesystem's **sync** operation for data consistency.
This sync operation causes not performace overhead.
Specifically, RocksDB (one of KV-Stores) uses **fdatasync()** operation to synchronously write the disk for consistency.

In 2022, [NobLSM (DAC'22)](https://dl.acm.org/doi/abs/10.1145/3489517.3530470) has been published. The NobLSM claims if KV-Store levereages the Ext4 journaling infrastructure to write the KV data without inconsistency issues, the sync overhead can be removed since it writes in non-blocking write manner.

To validate this idea, NobLSM must be equipped with the following user/kernel-level data structures and components:
 - User space
    - NobLSM's SSTable Manager: For tracking SStable dependency
    - Compaction thread must call NobLSM-supported new syscalls (**check_commit** and **is_committed**).
 - Kernel space
    - Pending Table: For managing tracked SSTable
    - Committed Table: For managing inode whose the commit completes
    - check_commit syscall: inode tracking syscall 462
    - is_committed syscall: Check commit status
    - commit callback: when commit done, status transition (with Pending to Committed Table)

In this repo, user-space (RocksDB) and kernel-space (linux-6.8) has been modified and developed for validating the idea of NobLSM. Accoding to my experiment, NobLSM is superior to original RocksDB in terms of both throughput and latency perspectives.


<img src="../assets/noblsm_throughput.png" width="400">
<img src="../assets/noblsm_latency.png" width="400">


The throughput increased by up to 59.5\% and the latency reduced by up to 37.4\% for 10 millon KV *fillrandom* workload.
This is because major compaction does not rely on synchrounous disk write anymore. 

# Setup
- OS: Ubuntu 20.04 or 22.04
- Kernel: 6.8
- RocksDB: v8.11.4
- Compiler: GCC 9+ / CMake 3.16+


This project integrates:
- Linux kernel (GPL-2.0 WITH Linux-syscall-note): https://github.com/anonymousresearcher01/linux/tree/NobLSM-v1.0 
    - Use `expr-tested` tagged version
- RocksDB (Apache-2.0): https://github.com/anonymousresearcher01/noblsm-dev-rocksdb

License:
- This project is released under the GNU General Public License v2 (GPLv2).
- Linux kernel retains its GPL-2.0 WITH Linux-syscall-note license.
- RocksDB retains its Apache-2.0 license.

# Reference
- RocksDB Wiki: https://github.com/facebook/rocksdb/wiki
- Linux kernel reference: https://elixir.bootlin.com
- JBD2: [linux/Documentation/filesystems/ext4/](https://github.com/torvalds/linux/tree/master/Documentation/filesystems/ext4)
- NobLSM (DAC '22) Paper: https://dl.acm.org/doi/abs/10.1145/3489517.3530470
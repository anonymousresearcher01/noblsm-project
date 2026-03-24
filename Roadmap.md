# Implementation Roadmap

## User-space RocksDB

[x] Remove `fsync()` during major compaction of RocksDB
[x] Design RocksDB to use `check_commit` and `is_committed` system calls.
[ ] Validate pending / committed status.

## Kernel-space (Ext4 + Syscall)

[x] Implement Linux kernel module for pending and commited table
[x] Add system calls
[x] Handle inode in JBD2 commit hook
[ ] Connect syscalls
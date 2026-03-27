# RocksDB Setup

## Build
```bash
git clone https://github.com/facebook/rocksdb.git
cd rocksdb
git checkout v8.11.4

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DWITH_GFLAGS=ON \
         -DWITH_SNAPPY=ON
# 추후 Release 로 테스트
# cmake .. -DCMAKE_BUILD_TYPE=Release \
#          -DWITH_GFLAGS=ON \
#          -DWITH_SNAPPY=ON
make -j$(nproc) # Due to my resource; make-j4
```
## Test
```bash
./db_bench --benchmarks=fillrandom \
           --num=1000000 \
           --value_size=1024 \
           --db=/tmp/rocksdb_test
```

# 1. RocksDB's fsync() calls check

```bash
git log --oneline ~/rocksdb/db/compaction/compaction_outputs.cc
```

1. TableBuilder 완료 시점 (builder.cc:413) minor compaction 시 SSTable 생성 후, sync 호출
2. compaction_job.cc:3154 major compaction 후 sync 호출

## The Known
- 기본적으로 RocksDB 에서 `use_fsync = false` 여서 `fdatasync` 사용
- builder.cc:413 에서 minor & major compaction 모두 SSTable 생성 시 sync 호출
- compaction_job.cc:2076 에서 major compaction SSTable sync (NobLSM 의 제거 대상) using `WriterSyncClose()`
- `SyncManifest()` 에서 Sync() 제거

### Details of `WriterSyncClose()`
file_writer_ -> Sync() 에서 실제 `fdatasync()` 호출.
여기서, Sync() 스킵하고 inode 를 Pending Table 에 등록한 뒤, JBD2 활용하는 전략.

### Details of `SyncManifest()`
Remove manifest sync

```c++
IOStatus SyncManifest(const ImmutableDBOptions* db_options,
                      const WriteOptions& write_options,
                      WritableFileWriter* file) {
  TEST_KILL_RANDOM_WITH_WEIGHT("SyncManifest:0", REDUCE_ODDS2);
  (void)db_options;
  (void)write_options;(void)file;
  return IOStatus::OK();
}
```


# 2. Mock Syscall & Data structure of RocksDB
`roksdb/nob` directory 에서 목킹 역할 시스템콜 (nob_mock_syscall.h), Shadow SSTalbe (nob_sstable_manager.h) 생성

### Kernel-space (JBD2)
- check_commit(inode): Ext4 Pending Table 에 inode 등록
- is_committed(inode): Ext4 Committed Table 에 있는지 확인 (Checkpoint 여부 확인)

### User-space (Mocking)
- nob_mock_commit(ino): Pending set 추가 & 백그라운드 5초 후 자동 Committed 이동 
- nob_is_committed(ino): Committed 에 있으면 true

흐름:
```
(1) major compaction 완료
(2) RegisterCompaction(old_ssts, new_ssts) 호출
(3) old_ssts → shadow_set_에 추가 (검색 제외)
    new_ssts → nob_check_commit() 호출 (Ext4 추적 시작)
(4) ... 5초마다 TryReclaimObsolete() 호출
(5) 모든 new_ssts가 is_committed == true
(6) old_ssts 파일 삭제 + shadow_set_에서 제거
```

### Mocking Test
```c++
/* /tmp/test_nob.cc */
#include "/home/dhmin/rocksdb/nob/nob_mock_syscall.h"
#include "/home/dhmin/rocksdb/nob/nob_sstable_manager.h"
#include <iostream>

int main() {
    using namespace ROCKSDB_NAMESPACE::nob;

    nob_check_commit(1001);
    nob_check_commit(1002);
    std::cout << "Registered inodes 1001, 1002 to pending\n";

    std::cout << "1001 committed? " << nob_is_committed(1001) << "\n";

    auto& mgr = NobSSTableManager::GetInstance();
    mgr.RegisterCompaction({101, 102}, {201, 202});
    std::cout << "Pending deps: " << mgr.PendingCount() << "\n";
    std::cout << "101 is shadow? " << mgr.IsShadow(101) << "\n";

    std::cout << "OK\n";
    return 0;
}
```
```bash
g++ -std=c++17 -pthread /tmp/test_nob.cc -o /tmp/test_nob
/tmp/test_nob
sudo dmesg | grep "NobLSM" | tail -20 # For checking syscall
```

### Test Result
```bash
dhmin@dhmin:~/rocksdb/nob$ g++ -std=c++17 -pthread /tmp/test_nob.cc -o /tmp/test_nob && /tmp/test_nob
Registered inodes 1001, 1002 to pending
1001 committed? 0
Pending deps: 1
101 is shadow? 1
OK
```

# 3. `compaction_outputs.h` & `compaction_outputs.cc` 생성 및 `compaction_job.cc` 연결
(1) add NobLSM Method
```c++
IOStatus NobWriterClose(const Status& input_status);
```

(2) WriterSyncClose() without sync
```c++
IOStatus CompactionOutputs::NobWriterClose(const Status& input_status) {
  IOStatus io_s;
  IOOptions opts;
  io_s = WritableFileWriter::PrepareIOOptions(
      WriteOptions(Env::IOActivity::kCompaction), opts);

  if (input_status.ok() && io_s.ok()) {
    io_s = file_writer_->Close(opts);
  }
  if (input_status.ok() && io_s.ok()) {
    FileMetaData* meta = GetMetaData();
    meta->file_checksum = file_writer_->GetFileChecksum();
    meta->file_checksum_func_name = file_writer_->GetFileChecksumFuncName();
  }
  file_writer_.reset();
  return io_s;
}
```

(3) replace `WriterSyncClose()` in compaction_job.cc
```c++
// Finish and check for file errors
// NobLSM: major compaction은 sync 없이 close, Ext4 async commit에 위임
IOStatus io_s = outputs.NobWriterClose(s);

if (io_s.ok() && s.ok()) {
uint64_t new_file_num = meta->fd.GetNumber();
nob::NobSSTableManager::GetInstance().RegisterCompaction(
    {},  
    {new_file_num});
}
```

# 4. (After registering and developing syscall) Replace the mocked with actual syscall

```c++
/* nob/nob_mock_syscall.h */
#include <sys/syscall.h>
#include <unistd.h>

#define SYS_NOB_CHECK_COMMIT 462
#define SYS_NOB_IS_COMMITTED 463

inline void nob_check_commit(InodeNum ino) {
    syscall(SYS_NOB_CHECK_COMMIT, ino);
}

inline bool nob_is_committed(InodeNum ino) {
    return syscall(SYS_NOB_IS_COMMITTED, ino) == 1;
}
```

Replace code of `WriterSyncClose()` again in compaction_job.cc with below
```c++
#include <sys/stat.h>
...
(중략)
...

if (io_s.ok() && s.ok()) {
    // 실제 파일의 inode 번호 가져오기
    std::string fname = GetTableFileName(meta->fd.GetNumber());
    struct stat st;
    if (::stat(fname.c_str(), &st) == 0) {
        nob::NobSSTableManager::GetInstance().RegisterCompaction(
            {},
            {static_cast<uint64_t>(st.st_ino)});
        nob::nob_check_commit(static_cast<uint64_t>(st.st_ino));
    }
}
```



# 5. (Optional) Copy RocksDB to VM (Linux dev env)
지금 VM 셋업이 로컬에서 2222 포트 포워딩해둔 상태이므로 아래 명령어

```bash
rsync -av ~/rocksdb/ dev@localhost -p 2222:~/rocksdb/ \
    --exclude=build
```


# 6. Build & Test
```bash
cd ~/rocksdb/build
make -j$(nproc) db_bench 2>&1 | head -50

# Original RocksDB
~/rocksdb/build_orig/db_bench \
    --benchmarks=fillrandom \
    --num=10000000 \
    --value_size=1024 \
    --db=/tmp/orig_10m 2>&1 | grep "fillrandom"

# NobLSM
~/rocksdb/build/db_bench \
    --benchmarks=fillrandom \
    --num=10000000 \
    --value_size=1024 \
    --db=/tmp/nob_10m 2>&1 | grep "fillrandom"

# Remove db file after experiments
rm -rf /tmp/orig_10m /tmp/nob_10m
```

Flow:

  - pending_add(inode) (=RocksDB check_commit syscall)
  - pending_contains(inode)=1 (=ext4_writepages 에서 확인)
  - registering callback (=콜백 등록)
  - commit callback rc=0 (=Ext4 async commit 완료)
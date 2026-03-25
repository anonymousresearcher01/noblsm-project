# NobLSM Development Plan
    (1) Ext4 journal commit
    (2) then `j_commit_callback` trigger
    (3) inode to Pending Table
    (4) Then Committed Table
    (5) `is_committed` check from RocksdB
    (6) Delete old SSTable

# NobLSM Implementation
## Kernel Files to be modified
 - Pending / Committed Table
    - `fs/ext4/nob_commit_tracker.h`
    - `fs/ext4/nob_commit_tracker.c`
 - check_commit / is_committed syscall
    - `fs/ext4/nob_syscall.c`
 - syscall table
    - `arch/x86/entry/syscalls/syscall_64.tbl`
 - callback register
    - `fs/ext4/super.c`
 - ext4_write_begin() -> nob_register_callback() trigger
    - `fs/ext4/inode.c`

### IMPORTANT
    파일 write 시점 (journal handle 존재)
        → nob_register_callback(handle, ino) 호출
        → Ext4 t_private_list에 콜백 등록
        → commit 완료 시 자동으로 nob_move_to_committed() 호출

### `fs/ext4/nob_commit_tracker.h`
Implementation of Pending and Committed Table.

 - Declare `nob_pending_entry` (inode / hlist_node)
 - Declare `nob_committed_entry` 
 - Declare `nob_commit_tracker` (w/ spinlock)
 - Declare `nob_cb_entry` (NobLSM-dedicated callback structure entry)

### `fs/ext4/nob_commit_tracker.c`
 - Implement `nob_tracker_init` for initializing hash and spinlock
 - Implement `nob_pending_add` for adding inode to Pending Table
 - Implement `nob_is_committed` for checking whether the inode is in the Committed Table
 - Implement `nob_move_to_committed` for transitioning Pending to Committed Table
 - Implement `nob_committed_erase` for old SSTable's inode from Committed Table
 - Implement `nob_commit_callback` and `nob_register_callback`
 - Implement `nob_pending_contains` for checking whether the pending contains inodes

###  `fs/ext4/super.c`
 - Before `ext4_journal_commit_callback`, call `nob_tracker_init()`!!

### `arch/x86/entry/syscalls/syscall_64.tbl`
 - Add system call (nob_check_commit & nob_is_committed) (sysnum: 462 / 463)
 - Add syscalls to include/linux/syscalls.h
    ```c
    asmlinkage long sys_nob_check_commit(unsigned long ino);
    asmlinkage long sys_nob_is_committed(unsigned long ino);
    ```
### `fs/ext4/nob_syscall.c`
 - Implement `SYSCALL_DEFINE1(nob_check_commit, unsigned long, ino)`
 - Implement `SYSCALL_DEFINE1(nob_is_committed, unsigned long, ino)`

### `fs/ext4/inode.c`
In `ext4_writepages()`, before calling `ext4_do_writepages()`, callback register!

New
--

```c
if (nob_pending_contains(mpd.inode->i_ino)) {
    handle_t *nob_handle;
    nob_handle = ext4_journal_start(mpd.inode, EXT4_HT_WRITE_PAGE, 1);
    if (!IS_ERR(nob_handle)) {
        pr_info("NobLSM: registering callback in writepages ino=%lu\n", mpd.inode->i_ino);
        nob_register_callback(nob_handle, mpd.inode->i_ino);
        ext4_journal_stop(nob_handle);
    }
}
ext4_do_writepages(&mpd)
```


Obsolete
--

```c
handle = ext4_journal_start(inode, EXT4_HT_WRITE_PAGE, needed_blocks);
if (IS_ERR(handle)) {
    folio_put(folio);
    return PTR_ERR(handle);
}

/* NobLSM */
if (nob_pending_contains(inode->i_ino))
    pr_info("NobLSM: registering callback for inode %lu\n", inode->i_ino);
    nob_register_callback(handle, inode->i_ino);
```

### `fs/ext4/Makefile`
 - Add `nob_commit_tracker.o`
 - Add `nob_syscall.o`



```c
nob_tracker_init();

if (sbi->s_journal)
    sbi->s_journal->j_commit_callback = ext4_journal_commit_callback;
```
    (1) Ext4 fs mount
    (2) ext4_fill_super() # Kernel initializes ext4
    (3) call not_tracker_init()
    (4) Pending / Committed Table

## Source code Build and Running new kernel
```bash
cd ~/linux-6.8
# make -j$(nproc) 2>&1 | tee /tmp/build.log
make -j$(nproc) 2>&1 | grep -E "error:|Error" | grep -v "error-inject\|error_private\|uterror\|utxferror\|notifier-error\|vf_error\|gpu_error\|scsi_error\|fme-error\|afu-error" | head -20

# For checking
ls -lh ~/linux-6.8/arch/x86/boot/bzImage

# For nob module 
find ~/linux-6.8 -name "nob_commit_tracker.o" 2>/dev/null

# kernel module install and make install then reboot the system
sudo make modules_install
sudo make install

# If old initrd must be deleted, then do below.
# sudo rm /boot/initrd.img-6.8.0

ls /boot/vmlinuz-6.8.0
sudo update-grub
sudo reboot

# After booting, check whether the NobLSM module has been loaded or not
dmesg | grep -i "nob\|NobLSM"
```

# NobLSM Test Code
```c
/* test_nob_syscall.c */
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SYS_NOB_CHECK_COMMIT 462
#define SYS_NOB_IS_COMMITTED 463

int main() {
    struct stat st;

    int fd = open("/tmp/test_nob2.txt", O_CREAT|O_WRONLY, 0644);
    close(fd);

    stat("/tmp/test_nob2.txt", &st);
    unsigned long ino = st.st_ino;
    printf("inode: %lu\n", ino);

    // Call check_commit
    long ret = syscall(SYS_NOB_CHECK_COMMIT, ino);
    printf("check_commit(%lu) = %ld\n", ino, ret);

    fd = open("/tmp/test_nob2.txt", O_WRONLY|O_APPEND);
    write(fd, "NobLSM test\n", 12);
    close(fd);

    // Confirm inode 
    stat("/tmp/test_nob2.txt", &st);
    printf("inode after write: %lu (same? %d)\n", st.st_ino, ino == st.st_ino);

    ret = syscall(SYS_NOB_IS_COMMITTED, ino);
    printf("is_committed = %ld (expected: 0)\n", ret);

    // Maybe 5 sec is enough but as a grace period.
    printf("Waiting 7 seconds...\n");
    sleep(7);

    ret = syscall(SYS_NOB_IS_COMMITTED, ino);
    printf("is_committed = %ld (expected: 1)\n", ret);

    return 0;
}
```

```bash
gcc -o ~/test_nob_syscall ~/test_nob_syscall.c && ~/test_nob_syscall
sudo dmesg | grep -i "NobLSM" | tail -10

# Result
dev@kernel-dev:~$ ./test_nob_syscall 
inode: 4063252
check_commit(4063252) = 0
inode after write: 4063252 (same? 1)
is_committed = 0 (expected: 0)
Waiting 7 seconds...
is_committed = 0 (expected: 1)
dev@kernel-dev:~$ sudo dmesg |grep "NobLSM"
[    7.113066] NobLSM: commit tracker initialized
[   26.155720] NobLSM: pending_add inode 4063251
[  133.913457] NobLSM: pending_add inode 4063251
[  209.960911] NobLSM: pending_add inode 4063252
```
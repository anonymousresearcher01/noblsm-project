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
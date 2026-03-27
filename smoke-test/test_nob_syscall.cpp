#include <iostream>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <thread>
#include <chrono>

#define SYS_NOB_CHECK_COMMIT 462
#define SYS_NOB_IS_COMMITTED 463

class FileCommitTracker {
    std::string filepath;
    unsigned long inode;

public:
    FileCommitTracker(const std::string& path) : filepath(path) {
        int fd = open(filepath.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fd == -1) {
            perror("open");
            throw std::runtime_error("Failed to create file");
        }
        close(fd);

        struct stat st;
        if (stat(filepath.c_str(), &st) == -1) {
            perror("stat");
            throw std::runtime_error("Failed to stat file");
        }
        inode = st.st_ino;
    }

    unsigned long getInode() const { return inode; }

    long checkCommit() const {
        long ret = syscall(SYS_NOB_CHECK_COMMIT, inode);
        std::cout << "check_commit(" << inode << ") = " << ret << std::endl;
        return ret;
    }

    void appendData(const std::string& data) const {
        int fd = open(filepath.c_str(), O_WRONLY | O_APPEND);
        if (fd == -1) {
            perror("open append");
            throw std::runtime_error("Failed to open file for append");
        }
        if (write(fd, data.c_str(), data.size()) == -1) {
            perror("write");
            close(fd);
            throw std::runtime_error("Failed to write data");
        }
        close(fd);
    }

    void printInode() const {
        struct stat st;
        if (stat(filepath.c_str(), &st) == -1) {
            perror("stat");
            throw std::runtime_error("Failed to stat file");
        }
        std::cout << "inode after write: " << st.st_ino
                  << " (same? " << (st.st_ino == inode) << ")\n";
    }

    long isCommitted() const {
        long ret = syscall(SYS_NOB_IS_COMMITTED, inode);
        std::cout << "is_committed = " << ret << std::endl;
        return ret;
    }

    void waitForCommit(int seconds) const {
        std::cout << "Waiting " << seconds << " seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }
};

int main() {
    try {
        FileCommitTracker tracker("/tmp/test_nob2.txt");

        std::cout << "inode: " << tracker.getInode() << std::endl;

        tracker.checkCommit();

        tracker.appendData("NobLSM test\n");
        tracker.printInode();

        tracker.isCommitted(); // expected 0

        tracker.waitForCommit(7);

        tracker.isCommitted(); // expected 1
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
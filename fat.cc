#include "fat_internal.h"

bool fat_mount(const std::string &path) {
    return false;
}

int fat_open(const std::string &path) {
    return -1;
}

bool fat_close(int fd) {
    return false;
}

int fat_pread(int fd, void *buffer, int count, int offset) {
    return -1;
}

std::vector<AnyDirEntry> fat_readdir(const std::string &path) {
    std::vector<AnyDirEntry> result;
    return result;
}

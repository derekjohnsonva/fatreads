#include "fat_internal.h"
#include <iostream>


bool is_eof(uint32_t fat_content){
    if (fat_content >= 0x0FFFFFF8)
        return true;
    return false;
}

std::vector<int> get_clusters_from_fat(int cluster_num) {
    std::vector<int> cluster_nums;
    while (cluster_num < 0x0FFFFFF8){
        cluster_nums.push_back(cluster_num);
        cluster_num = fatTable[cluster_num] & 0x0FFFFFFF;
    }
    return cluster_nums;
}

std::vector<DirEntry> read_cluster(int cluster_num) {
    std::vector<int> cluster_nums = get_clusters_from_fat(cluster_num);
    std::vector<DirEntry> dirEntries;
    for(int cluster : cluster_nums){
        uint32_t first_sector_of_cluster = ((cluster - 2) * fatbpb->BPB_SecPerClus) + first_data_sector;
        infile.seekg(first_sector_of_cluster * fatbpb->BPB_BytsPerSec);
        char *cur_cluster = (char *)malloc(cluster_size * sizeof(char));
        infile.read(cur_cluster, cluster_size);
        uint32_t cur_entry = 0;
        while(cur_entry * dir_entry_size < cluster_size){
            // get the first byte of the entry
            char firstByte = cur_cluster[cur_entry * dir_entry_size];
            if(firstByte == 0x0){
                break;
            }
            if(firstByte != 0xE5){
                DirEntry *newDirEntry = (DirEntry *)&(cur_cluster[cur_entry * dir_entry_size]);
                dirEntries.push_back(*newDirEntry);
            }
            ++cur_entry;
        }
    }
    return dirEntries;
}


bool fat_mount(const std::string &path) {
    // Load the BPB
    infile.open(path, std::ifstream::in | std::ifstream::binary);
    if(infile.bad()){
        // the file could not be opened
        return false;
    }
    int bpb_size = int(sizeof(Fat32BPB));
    char *in_bpb = (char *)malloc(bpb_size);
    if (!infile.read(in_bpb, bpb_size)){
        infile.clear();
        infile.close();
        return false;
    }
    fatbpb = (Fat32BPB *) in_bpb;
    // set data for the file
    root_dir_sectors = ((fatbpb->BPB_rootEntCnt * 32) + (fatbpb->BPB_BytsPerSec - 1)) / fatbpb->BPB_BytsPerSec;
    first_data_sector = fatbpb->BPB_RsvdSecCnt + (fatbpb->BPB_NumFATs * fatbpb->BPB_FATSz32) + root_dir_sectors;
    first_fat_sector = fatbpb->BPB_RsvdSecCnt;
    data_sec = fatbpb->BPB_TotSec32 - (fatbpb->BPB_RsvdSecCnt +(fatbpb->BPB_NumFATs * fatbpb->BPB_FATSz32) + root_dir_sectors);
    cluster_size = fatbpb->BPB_SecPerClus * fatbpb->BPB_BytsPerSec;
    count_of_clusters = data_sec / fatbpb->BPB_SecPerClus;
    root_cluster_32 = fatbpb->BPB_RootClus;
    dir_entry_size = cluster_size / sizeof(DirEntry);

    int bytes_per_fat = fatbpb->BPB_BytsPerSec * fatbpb->BPB_FATSz32;
    // go to location of the fat table
    infile.seekg(fatbpb->BPB_RsvdSecCnt * fatbpb->BPB_BytsPerSec);
    fatTable = (uint32_t *)malloc(bytes_per_fat);
    if(!infile.read((char *)fatTable, bytes_per_fat)){
        std::cerr << "could not read fat";
        return false;
    }
    return true;
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
    if(!infile.is_open()){  // check if a file is mounted
        return result;
    }
    std::vector<DirEntry> rootDir;
    rootDir = read_cluster(root_cluster_32);
    printf("I have finished the call\n");
    for(DirEntry dir : rootDir){
       AnyDirEntry ade;
        ade.dir = dir;
        result.push_back(ade); 
    }
    return result;
}

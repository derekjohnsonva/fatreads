#include "fat_internal.h"
#include <iostream>
#include <sstream>
#include <algorithm>

bool str_equals(const std::string& a, const std::string& b)
{
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
                      [](char a, char b) {
                          return tolower(a) == tolower(b);
                      });
}

// removes whitespace from a string
void remove_space(std::string &s){
    s.erase(remove_if(s.begin(), s.end(), isspace), s.end());
}

bool dir_matches_name(DirEntry &dir, std::string dir_name){
    std::string name(&dir.DIR_Name[0], &dir.DIR_Name[11]);
    remove_space(name);
    if(str_equals(dir_name, name)) return true;

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
    char *cur_cluster = (char *)malloc(cluster_size * sizeof(char));
    for(int cluster : cluster_nums){
        uint32_t first_sector_of_cluster = ((cluster - 2) * fatbpb->BPB_SecPerClus) + first_data_sector;
        infile.seekg(first_sector_of_cluster * fatbpb->BPB_BytsPerSec);
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
    free(cur_cluster);
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
    dir_entry_size = 32;//cluster_size / sizeof(DirEntry);

    int bytes_per_fat = fatbpb->BPB_BytsPerSec * fatbpb->BPB_FATSz32;
    // go to location of the fat table
    infile.seekg(fatbpb->BPB_RsvdSecCnt * fatbpb->BPB_BytsPerSec);
    fatTable = (uint32_t *)malloc(bytes_per_fat);
    if(!infile.read((char *)fatTable, bytes_per_fat)){
        std::cerr << "could not read fat\n";
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

uint32_t get_dir_cluster_num(DirEntry &dir){
    uint32_t combine = ((unsigned int) dir.DIR_FstClusHI << 16) + ((unsigned int) dir.DIR_FstClusLO);
    return combine;
}

// Check to make sure first char of path is "/"
bool is_root_ref(const std::string &path){
    if (path.at(0) != '/') {
        return false;
    }
    return true;
}

// Should split the path by "/" tokens
std::vector<std::string> &split_path(const std::string &path, std::vector<std::string> &elems) {
    std::stringstream ss(path.substr(1)); // ignore the first val as it should be a '/'
    std::string item;
    while(std::getline(ss, item, '/')) {
        elems.push_back(item);
    }
    return elems;
}

bool get_dir_entry(uint32_t cluster_num, std::string dir_name, DirEntry &dir){
    std::vector<int> cluster_nums = get_clusters_from_fat(cluster_num);
    char *cur_cluster = (char *)malloc(cluster_size * sizeof(char));
    for(int cluster : cluster_nums){
        uint32_t first_sector_of_cluster = ((cluster - 2) * fatbpb->BPB_SecPerClus) + first_data_sector;
        infile.seekg(first_sector_of_cluster * fatbpb->BPB_BytsPerSec);
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
                if(dir_matches_name(*newDirEntry, dir_name)){
                    dir = *newDirEntry;
                    free(cur_cluster);
                    return true;
                }
            }
            ++cur_entry;
        }
    }
    free(cur_cluster);
    return false;
}

std::vector<AnyDirEntry> fat_readdir(const std::string &path) {
    std::vector<AnyDirEntry> result;
    if(!infile.is_open()){  // check if a file is mounted
        return result;
    }
    if(!is_root_ref(path)){
        std::cerr << "trying to read a path that is not indexed from the root\n";
        return result;
    }
    
    std::vector<std::string> path_dirs;
    split_path(path, path_dirs);

    uint32_t cur_folder_cluster = root_cluster_32;
    for(int i = 0; i <= (int)path_dirs.size(); i++){
        if(cur_folder_cluster == 0) cur_folder_cluster = root_cluster_32;
        if(i == (int) path_dirs.size()){
            std::vector<DirEntry> dir_entries;
            dir_entries = read_cluster(cur_folder_cluster);
            for(DirEntry dir : dir_entries){
                AnyDirEntry ade;
                ade.dir = dir;
                result.push_back(ade); 
            }
        } else {
            DirEntry next_folder;
            std::string dir_name = path_dirs.at(i);
            bool found_folder = get_dir_entry(cur_folder_cluster, dir_name, next_folder);
            if(!found_folder){
                std::cerr << "could not find folder with name " << dir_name << "\n";
                return result;
            }
            cur_folder_cluster = get_dir_cluster_num(next_folder);
        }

    }
    return result;
}

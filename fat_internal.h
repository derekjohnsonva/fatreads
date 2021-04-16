#ifndef FAT_INTERNAL_H_
#define FAT_INTERNAL_H_
#include "fat.h"

/*
 * On a little-endian machine, this struct duplicates the on-disk layout of the FAT32
 * Boot Sector and Bios Parameter Block (BPB), described in pages 7-13 of the FAT specification.
 *
 * Since this has the exact same layout, you should be able to read directly from the disk image
 * into this struct.
 */

struct __attribute__((packed)) Fat32BPB {
    uint8_t BS_jmpBoot[3];          // jmp instr to boot code
    uint8_t BS_oemName[8];          // indicates what system formatted this field, default=MSWIN4.1
    uint16_t BPB_BytsPerSec;       // Count of bytes per sector
    uint8_t BPB_SecPerClus;         // no.of sectors per allocation unit
    uint16_t BPB_RsvdSecCnt;        // no.of reserved sectors in the resercved region of the volume starting at 1st sector
    uint8_t BPB_NumFATs;            // The count of FAT datastructures on the volume
    uint16_t BPB_rootEntCnt;        // Count of 32-byte entries in root dir, for FAT32 set to 0
    uint16_t BPB_totSec16;          // total sectors on the volume
    uint8_t BPB_media;              // value of fixed media
    uint16_t BPB_FATSz16;           // count of sectors occupied by one FAT
    uint16_t BPB_SecPerTrk;         // sectors per track for interrupt 0x13, only for special devices
    uint16_t BPB_NumHeads;          // no.of heads for intettupr 0x13
    uint32_t BPB_HiddSec;           // count of hidden sectors
    uint32_t BPB_TotSec32;          // count of sectors on volume
    uint32_t BPB_FATSz32;           // define for FAT32 only
    uint16_t BPB_ExtFlags;          // flags indicating which FATs are used
    uint16_t BPB_FSVer;             // Major/Minor version num
    uint32_t BPB_RootClus;          // Clus num of 1st clus of root dir
    uint16_t BPB_FSInfo;            // sec num of FSINFO struct
    uint16_t BPB_bkBootSec;         // copy of boot record
    uint8_t BPB_reserved[12];       // reserved for future expansion
    uint8_t BS_DrvNum;              // drive num
    uint8_t BS_Reserved1;           // for ue by NT
    uint8_t BS_BootSig;             // extended boot signature
    uint32_t BS_VolID;              // volume serial number
    uint8_t BS_VolLab[11];          // volume label
    uint8_t BS_FileSysTye[8];       // FAT12, FAT16 etc
};

#endif

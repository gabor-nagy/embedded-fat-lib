/** \file fat.h
\brief FAT library header file.

High level functions, compiler directives, constants, error codes, structures,
etc. that should be used to handle a FAT12/16/32 file system volume. 

*/
#ifndef FS_FAT_H
#define FS_FAT_H

#include "interface.h"

/* settings */

/** Controls whether to compile FAT12 specific program parts. */
#define FS_FAT_FAT12_ENABLE
/** Controls whether to compile FAT16 specific program parts. */
#define FS_FAT_FAT16_ENABLE
/** Controls whether to compile FAT32 specific program parts. */
#define FS_FAT_FAT32_ENABLE

/** Controls whether to support Kanji character set. */
#define FS_FAT_KANJI

/* macros */

#ifdef FS_FAT_FAT12_ENABLE
#define FS_FAT_FAT12_16_ENABLE
#endif

#ifdef FS_FAT_FAT16_ENABLE
#define FS_FAT_FAT12_16_ENABLE
#endif

#define ATTR_READ_ONLY	0x01
#define ATTR_HIDDEN  0x02
#define ATTR_SYSTEM	0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME	(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | \
	ATTR_VOLUME_ID)
#define ATTR_LONG_NAME_MASK	(ATTR_LONG_NAME | ATTR_DIRECTORY | ATTR_ARCHIVE)

/* error codes */

#define FS_FAT_ERR_DISKFULL	-1
#define FS_FAT_ERR_DIRFULL  -2

/* type definitions */

typedef struct {
   u8	jmpBoot[3];
   u8	OEMName[8];
   u16	BytsPerSec;
   u8	SecPerClus;
   u16	RsvdSecCnt;
   u8	NumFATs;
   u16	RootEntCnt;
   u16	TotSec16;
   u8	Media;
   u16	FATSz16;
   u16	SecPerTrk;
   u16	NumHeads;
   u32	HiddSec;
   u32	TotSec32;
} __attribute__ ((packed)) t_bpb;

typedef struct {
	u8	DIR_Name[11];
	u8	DIR_Attr;
	u8	DIR_NTRes; /* unused */
	u8	DIR_CrtTimeTenth;
	u16	DIR_CrtTime;
	u16	DIR_CrtDate;
	u16   DIR_LstAccDate;
	u16   DIR_FstClusHI; /* always 0 for FAT12/16 */
	u16   DIR_WrtTime;
	u16   DIR_WrtDate;
	u16   DIR_FstClusLO;
	u32   DIR_FileSize;
} __attribute__ ((packed)) t_directory;

#ifdef FS_FAT_FAT12_16_ENABLE

typedef struct {
   u8	DrvNum;
   u8	Reserved1;
   u8	BootSig;
   u32	VolID;
   u8	VolLab[11];
   u8	FilSysType[8];
} __attribute__ ((packed)) t_bpb12_16;

#endif

#ifdef FS_FAT_FAT32_ENABLE

typedef struct {
   u32	FATSz32;
   u16	ExtFlags;
   u16	FSVer;
   u32	RootClus;
   u16	FSInfo;
   u16	BkBootSec;
   u8	Reserved[12];
   u8	DrvNum;
   u8	Reserved1;
   u8	BootSig;
   u32	VolID;
   u8	VolLab[11];
   u8	FilSysType[8];
} __attribute__ ((packed)) t_bpb32;

typedef struct {
   /* TODO: page 21 */
} __attribute__ ((packed)) t_fsinfo;

#endif

/** File type to be used with library functions. */
typedef struct {
   t_directory desc;
   u32   pos;
   u32   cluster;
} __attribute__ ((packed)) t_file;

/** Change current directory.
\param directory Name of the directory.
\return Zero if error. */
s8 fs_fat_chdir(u8* directory);

/** Open file.
\param name Name of the file to be opened.
\param mode Similar to standard fopen(). At now, 'r' and 'w' are supported.
\param handle File handle to be used.
\return Zero if error. */
t_file *fs_fat_fopen(u8 *name, u8 mode, t_file *handle);

/** Binary read file.
\param handle File handle to be used.
\param dest_buffer Buffer to store data.
\param length Number of bytes to read at most.
\return Number of bytes read. */
u32 fs_fat_fread(t_file *handle, void *dest_buffer, u32 length);

/** Binary write file.
\param handle File handle to be used.
\param source_buffer Buffer to get data from.
\param length Number of bytes to write at most.
\return Number of bytes written. */
u32 fs_fat_fwrite(t_file *handle, void *source_buffer, u32 length);

/** Seek file to a position.
\param handle File handle to be used.
\param position The file offset to seek.
\return Zero if error. */
s8 fs_fat_fseek(t_file *handle, u32 position);

/** Get file size.
\param handle File handle to be used.
\warning There is no guarantee that the returned value reflects the correct
physical space occupied by the file on the disk. This value only returns the
size member of the directory entry which describes the file.
\return File size in bytes. */
u32 fs_fat_fsize(t_file *handle);

/** Erase file from the disk.
\param handle File handle to be used.
\return Zero if error. */
s8 fs_fat_ferase(u8 *name);

/** Resize file.
\param handle File handle to be used.
\param size New file size.
\return Zero if error. */
s8 fs_fat_fresize(t_file *handle, u32 size);

/** Close file.
\param handle File handle to be used.
\return Zero if error. */
s8 fs_fat_fclose(t_file *handle);

/** Returns a file's EOF state.
\param handle File handle to be used.
\return Non-zero if the file pointer reached the end of the file. */
s8 fs_fat_iseof(t_file *handle);

/** Return error code.

After any unsuccessful operation, this error code might help to understand what
the problem actually was.
\return Error code. */
s8 fs_fat_error_code();

#endif

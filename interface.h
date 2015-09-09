/** \file interface.h
\brief All of the abstract functions used by FAT library.

These few, simple functions should be implemented properly in order to make the
FAT library work correctly. No other user-implemented nor standard C library
functions are needed for working.

*/ 
#ifndef FS_FAT_INTERFACE_H
#define FS_FAT_INTERFACE_H

/* type definitions */

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned long int u32;

typedef signed char s8;
typedef signed short int s16;
typedef signed long int s32;

void *buffer, *fat_buffer; /* fat_buffer must be able to store 2 sectors in FAT12 */

/** Pointer to a function that reads one sector from storage device to a given
memory address.
\param lba LBA address of sector to read.
\param address Memory address to write to.
\return Zero if error. */
s8 (*fs_fat_read_sector_to_buffer)(u32, void *);

/** Pointer to a function that writes one sector to storage device from a given
memory address.
\param lba LBA address of sector to write.
\param address Memory address to read from.
\return Zero if error. */
s8 (*fs_fat_write_sector_from_buffer)(u32, void *);

/** Pointer to a function that copies num bytes of memory from source to
destination.
\param destination Destination memory address.
\param source Source memory address.
\param num Number of bytes to copy. */
void * (*fs_fat_memcpy)(void *destination, const void *source, u32 num);

#endif

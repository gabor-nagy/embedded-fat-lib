#include "fat.h"

/*---------------------------------*/

/* low-level functions */
s8 fs_fat_init();
u32 fs_fat_getentry(u32 index);
s8 fs_fat_setentry(u32 index, u32 value);
s8 fs_fat_iseoc(u32 entry);
u32 fs_fat_cluster_to_sec(u32 cluster);
u32 fs_fat_get_first_cluster(t_directory *dir);
u32 fs_fat_getfreeentry();
s8 fs_fat_read_sector(u32 lba);
s8 fs_fat_write_sector(u32 lba);
s8 fs_fat_read_sector_st(u32 sector);

/* file and directory handling functions */
t_directory *fs_fat_findfirstdirentry(t_directory **dir, u8 free);
t_directory *fs_fat_findfirst(t_directory **dir);
t_directory *fs_fat_findnextdirentry(t_directory **dir, u8 free);
t_directory *fs_fat_findnext(t_directory **dir);

t_file *fs_fat_create_file(t_file *file);

/* error handling */
void fs_fat_set_error_code(s8 ec);

/*---------------------------------*/

/* local 'define'-s */

#define EOC_MARK 0x0fffffff

/* global variables */

enum fat_types { fat12 = 12, fat16 = 16, fat32 = 32 } fat_type;

u8 num_fats;
u32 fat_size_sec;
u32 first_data_sec;
u32 data_sec_cnt;
u32 cluster_cnt;
u16 bytes_per_sec;
u16 sec_per_clus;
u32 root_dir_sec;
u8 resvd_sec_cnt;

#ifdef FS_FAT_FAT32_ENABLE
u16 root_dir_clus; /* TODO: do we really need that? */
#endif

#ifdef FS_FAT_FAT12_16_ENABLE
u16 root_ent_cnt;
#endif

u32 current_dir_entry;
u32 current_dir_sec;
t_directory current_dir_file_handle;
t_file current_dir_file;
u32 findnext_dir_sec;

u32 fat_free_entry;
u32 buffer_last_read_sector;
u32 error_code;

/* functions */

s8 fs_fat_init()
{
   t_bpb *bpb;
   #ifdef FS_FAT_FAT32_ENABLE
   t_bpb32 *bpb_32;
   #endif

   u32 RootDirSectors;
   u32 FATSz;
   u32 TotSec;
   
   bpb = (t_bpb *) buffer;
   
   /* read first sector */
   bytes_per_sec = 512; /* default value, a sector can only be larger */
   if (!fs_fat_read_sector_st(0)) return 0;
   
   /* TODO: check bad sector & signature */
   
   /* calculate root dir sector count */
   RootDirSectors =
       ((bpb -> RootEntCnt * sizeof(t_directory)) +
		 	(bpb -> BytsPerSec - 1)) / bpb -> BytsPerSec;
   
   if (bpb -> FATSz16 != 0) FATSz = bpb -> FATSz16;
   else
	{
		#ifdef FS_FAT_FAT32_ENABLE
		bpb_32 = (void *) bpb + sizeof(t_bpb);
		FATSz = bpb_32 -> FATSz32;
		#else
		return 0;
		#endif
	}
   
   if (bpb -> TotSec16 != 0) TotSec = bpb -> TotSec16;
   else TotSec = bpb -> TotSec32;
   
   resvd_sec_cnt = bpb -> RsvdSecCnt;
   
   /* calculate root first data sector */
   first_data_sec = (resvd_sec_cnt + (bpb -> NumFATs * FATSz) + RootDirSectors);
   /* TODO: add partition start sector (fatgen103.doc p.14) */
   data_sec_cnt = TotSec - first_data_sec;
		
   cluster_cnt = data_sec_cnt / bpb -> SecPerClus;
   
   /* determine FAT type */
   if (cluster_cnt < 4085)
	{
		fat_type = fat12;
		#ifndef FS_FAT_FAT12_ENABLE
		return 0;
		#endif
	}
   else if (cluster_cnt < 65525)
	{
		fat_type = fat16;
		#ifndef FS_FAT_FAT16_ENABLE
		return 0;
		#endif
	}
   else
	{
		fat_type = fat32;
		#ifndef FS_FAT_FAT32_ENABLE
		return 0;
		#endif
	}
   
   /* other settings */
   bytes_per_sec = bpb -> BytsPerSec;
   sec_per_clus = bpb -> SecPerClus;
   
   #ifdef FS_FAT_FAT32_ENABLE
   if (fat_type == fat32)
	{
		current_dir_file.cluster = root_dir_clus = bpb_32 -> RootClus;
        current_dir_file.pos = 0;
		current_dir_file_handle.DIR_FileSize = 0xffffffff;
		current_dir_file_handle.DIR_FstClusHI = root_dir_clus >> 16;
		current_dir_file_handle.DIR_FstClusLO = root_dir_clus & 0xffff;
		current_dir_file.desc = current_dir_file_handle;
		
		current_dir_sec = 0;
	}
	else
	#endif
	current_dir_sec =
		root_dir_sec = bpb -> RsvdSecCnt + (bpb -> NumFATs * bpb -> FATSz16);
	
	#ifdef FS_FAT_FAT12_16_ENABLE
	if (fat_type == fat12 || fat_type == fat16) root_ent_cnt = bpb -> RootEntCnt;
	#endif
	
	num_fats = bpb -> NumFATs;
	fat_size_sec = FATSz;
	
	fat_free_entry = 2; /* first 2 entries are reserved */
	
   return 1;
}

u32 fs_fat_getentry(u32 index)
{
	u32 bit_pos;
	#ifdef FS_FAT_FAT12_ENABLE
	u16 byte_pos;
	#endif
	
	/* determine a fat entry's bit position */
	
	#ifdef FS_FAT_FAT12_ENABLE
	if (fat_type == fat12)
	{
		bit_pos = index * 12;
	}
	#endif
	#ifdef FS_FAT_FAT16_ENABLE
	if (fat_type == fat16)
	{
		bit_pos = index * 16;
	}
	#endif
	#ifdef FS_FAT_FAT32_ENABLE
	if (fat_type == fat32)
	{
		bit_pos = index * 32;
	}
	#endif
	
	/* read the fat sector(s) containing the entry */
	fs_fat_read_sector_to_buffer(
			resvd_sec_cnt + (bit_pos / 8) / bytes_per_sec, fat_buffer);

	#ifdef FS_FAT_FAT12_ENABLE
	if (fat_type == fat12)
	{
		fs_fat_read_sector_to_buffer(
			resvd_sec_cnt + (bit_pos / 8) / bytes_per_sec + 1,
				((u8*)fat_buffer) + bytes_per_sec);
	}
	#endif
	
	bit_pos %= (bytes_per_sec * 8);
	
	/* retrieve and adjust result bits */
	#ifdef FS_FAT_FAT12_ENABLE
	if (fat_type == fat12)
	{
		if (bit_pos % 8 == 0)
		{
			return (((u8*) fat_buffer)[bit_pos / 8]) |
				(((((u8*) fat_buffer)[bit_pos / 8 + 1] & 0xf) << 8));
		}
		else
		{
			return ((((u8*) fat_buffer)[bit_pos / 8]) >> 4) |
				((((u8*) fat_buffer)[bit_pos / 8 + 1]) << 4);
		}
	}
	else
	#endif
	
	#ifdef FS_FAT_FAT16_ENABLE
	if (fat_type == fat16)
	{
		return ((u16*) fat_buffer)[bit_pos / 16];
	}
	else
	#endif
	
	#ifdef FS_FAT_FAT32_ENABLE
	if (fat_type == fat32)
	{
      return ((u32*) fat_buffer)[bit_pos / 32] & 0x0fffffff;
	}
	else
	#endif
	
	return 1; /* reserved cluster by default */
}

s8 fs_fat_setentry(u32 index, u32 value)
{
	u32 bit_pos;
	
	fs_fat_getentry(index);
	
	#ifdef FS_FAT_FAT12_ENABLE
	if (fat_type == fat12)
	{
      bit_pos = index * 12;
		if (bit_pos % 8 == 0)
		{
         ((u8*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 8] =
				(u8) (value & 0xff);
         ((u8*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 8 + 1] &= 0xf0;
         ((u8*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 8 + 1] |=
				(u8) ((value >> 8) & 0x0f);
		}
		else
		{
         ((u8*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 8] &= 0x0f;
         ((u8*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 8] |=
				(u8) ((value << 4) & 0xf0);
         ((u8*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 8 + 1] =
				(u8) (value >> 4);
		}
	}
	else
	#endif
	
	#ifdef FS_FAT_FAT16_ENABLE
	if (fat_type == fat16)
	{
      bit_pos = index * 16;
      ((u16*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 16] =
			(u16) (value & 0xffff);
	}
	else
	#endif
	
	#ifdef FS_FAT_FAT32_ENABLE
	if (fat_type == fat32)
	{
		bit_pos = index * 32;
		((u32*) fat_buffer)[(bit_pos % (bytes_per_sec * 8)) / 32] =
			value & 0x0fffffff;
	}
	else
	#endif
	
	return 0;
	
	fs_fat_write_sector_from_buffer(
			resvd_sec_cnt + (bit_pos / 8) / bytes_per_sec, fat_buffer);
   #ifdef FS_FAT_FAT12_ENABLE
   if (fat_type == fat12) /* TODO: condition: only write if sector boundary */
	{
      fs_fat_write_sector_from_buffer(
			resvd_sec_cnt + (bit_pos / 8) / bytes_per_sec + 1,
			((u8*) fat_buffer) + bytes_per_sec);
	}
	#endif
   
	return 1;
}

s8 fs_fat_iseoc(u32 entry)
{
   #ifdef FS_FAT_FAT12_ENABLE
	if (fat_type == fat12)
	{
		return (entry >= 0x0ff8);
	}
	#endif
	#ifdef FS_FAT_FAT16_ENABLE
	if (fat_type == fat16)
	{
		return (entry >= 0xfff8);
	}
	#endif
	#ifdef FS_FAT_FAT32_ENABLE
	if (fat_type == fat32)
	{
		return (entry >= 0x0ffffff8);
	}
	#endif
	
	return 1;
}

u32 fs_fat_cluster_to_sec(u32 cluster)
{
   return ((cluster - 2) * sec_per_clus) + first_data_sec;
}

u32 fs_fat_get_first_cluster(t_directory *dir)
{
	return (((u32) (dir -> DIR_FstClusHI)) << 16) | dir -> DIR_FstClusLO;
}

u32 fs_fat_getfreeentry()
{
	u32 start_pos = fat_free_entry; /* remember starting position */
	u8 eof_fat = 0;
	
   while ((fs_fat_getentry(fat_free_entry)
		#ifdef FS_FAT_FAT32_ENABLE
		& 0xfffffff
		#endif
		) != 0)
	{
		/* see whether we have at least one free entry */
		if (eof_fat && (fat_free_entry == start_pos))
		{
         /*printf("EOF FAT\n"); /* DEBUG */
			return 0; /* no more free space */
		}
		else
		{
			fat_free_entry++; /* try next entry */
		}

		if (fat_free_entry >= cluster_cnt) /* TODO: correct condition? */
		{
         fat_free_entry = 2; /* first two entries are reserved */
         eof_fat = 1; /* we have already reached the end */
		}
	}
	
	/*printf("RETURNED FREE ENTRY = %d\n", fat_free_entry); /* DEBUG */
	return fat_free_entry;
}

s8 fs_fat_read_sector(u32 lba)
{
   return read_sector_to_buffer(lba, buffer);
}

s8 fs_fat_write_sector(u32 lba)
{
   return write_sector_from_buffer(lba, buffer);
}

s8 fs_fat_read_sector_st(u32 sector)
{
   buffer_last_read_sector = sector;
   return fs_fat_read_sector(sector);
}

t_directory *fs_fat_findfirstdirentry(t_directory **dir, u8 free)
{
	current_dir_entry = 0;
	/* only if FAT32 root dir or FAT12/16 regular dir */
    if ((fat_type == fat32) || ((fat_type == fat12 || fat_type == fat16) &&
			(current_dir_sec != root_dir_sec)))
    {
       current_dir_file.cluster =
          fs_fat_get_first_cluster(&current_dir_file.desc);
       current_dir_file.pos = 0;
    }
    #ifdef FS_FAT_FAT12_16_ENABLE
    else findnext_dir_sec = current_dir_sec;
    #endif
	return fs_fat_findnextdirentry(dir, free);
}

t_directory *fs_fat_findfirst(t_directory **dir)
{
   return fs_fat_findfirstdirentry(dir, 0);
}

t_directory *fs_fat_findnextdirentry(t_directory **dir, u8 free)
{
	int eod = 0; /* end of directory */
	u8 good_entry;
	t_directory *dir_param = *dir;
	
	u8 first_char;

	do /* look directory region for appropriate directory entry */
	{
      #ifdef FS_FAT_FAT12_16_ENABLE
		if ((fat_type == fat12 || fat_type == fat16) &&
			(current_dir_sec == root_dir_sec))
		{
			/* root directory region */
			if (!fs_fat_read_sector_st(findnext_dir_sec)) return 0;
			*dir = ((t_directory *) buffer) + current_dir_entry;
			if ((++current_dir_entry) * sizeof(t_directory) == bytes_per_sec)
			{
            if (findnext_dir_sec - root_dir_sec + 1 <
						root_ent_cnt * sizeof(t_directory) / bytes_per_sec)
				{
					findnext_dir_sec++;
					current_dir_entry = 0;
					/*printf("next dir\n");*/
				}
				else eod = 1;
			}
		}
		#endif
			
		#ifdef FS_FAT_FAT12_16_ENABLE
		else
		#else
		if (fat_type == fat32)
		#endif
		{
			/* regular directory */
            fs_fat_fread(&current_dir_file, *dir, sizeof(t_directory));
            current_dir_entry++;
      	
      	    if (fs_fat_iseoc(current_dir_file.cluster))
   	        {
               eod = 1;
			}
		}

		first_char = (*dir) -> DIR_Name[0];
		
		#ifdef FS_FAT_KANJI
		if (first_char == 0x05) first_char = 0xe5;
		#endif

		if (!free)
		{
			good_entry = (first_char != 0xe5 && first_char != 0 &&
				((((*dir) -> DIR_Attr) & ATTR_LONG_NAME) != ATTR_LONG_NAME));
		}
		else
		{
         good_entry = (first_char == 0xe5 || first_char == 0);
		}
	}
	while (!good_entry && !eod);
	
	#ifdef FS_FAT_KANJI
	if (first_char == 0x05) (*dir) -> DIR_Name[0] = 0xe5;
	#endif
	
	if (!good_entry) *dir = 0;
	
	if (!eod)
	{
		if (current_dir_sec == root_dir_sec)
		{
         /* root directory region */
			fs_fat_memcpy(dir_param, *dir, sizeof(t_directory));
			*dir = dir_param;
		}
	}
	else *dir = 0;
	
	return *dir;
}

t_directory *fs_fat_findnext(t_directory **dir)
{
	return fs_fat_findnextdirentry(dir, 0);
}

s8 fs_fat_chdir(u8* directory)
{
   if (fs_fat_fopen(directory, 'r', &current_dir_file)) /* TODO: check directory flag */
   {
		#ifdef FS_FAT_FAT12_16_ENABLE
		if ((fat_type == fat12 || fat_type == fat16) && current_dir_file.cluster == 0)
		{
			current_dir_sec = root_dir_sec;
		}
		else
		#endif
		current_dir_sec = 0;
		
		current_dir_file.desc.DIR_FileSize = 0xffffffff;
		
		return 1;
	}
	else if (current_dir_sec == 0)
	{
		fs_fat_fseek(&current_dir_file, 0);
	}
	
	current_dir_entry = 0;
	
	return 0;
}

t_file *fs_fat_create_file(t_file *file)
{
	u32 j, ok, cluster;
   t_file f;

   if (fs_fat_fopen(file -> desc.DIR_Name, 'r', &f))
   {
		/* file does exist, try to erase it */
		fs_fat_fclose(&f);
		if (!fs_fat_ferase(file -> desc.DIR_Name)) return 0;
	}
   
   /* find a free directory entry */
   t_directory dir;
   t_directory *tdir = &dir;
   
   if (!fs_fat_findfirstdirentry(&tdir, 1))
   {
      fs_fat_set_error_code(FS_FAT_ERR_DIRFULL); /* error code */
      return 0;
   }
   
   do
   {
	   /* the empty t_directory is still in the buffer, modify it and write out */
		t_directory *dir_buffer = (t_directory *) buffer;

		for (j = 0; j < bytes_per_sec / sizeof(t_directory); j++)
		{
	      ok = 1;

			if (dir_buffer -> DIR_Name[0] == 0xe5 || dir_buffer -> DIR_Name[0] == 0)
	      {
				/* found the free entry */
	         cluster = fs_fat_getfreeentry();
	         
	         if (cluster == 0)
             {
                fs_fat_set_error_code(FS_FAT_ERR_DISKFULL);
                return 0; /* not enough free space */
             }

	         file -> desc.DIR_FstClusHI = (cluster >> 16);
	         file -> desc.DIR_FstClusLO = (cluster & 0xffff);
	         file -> cluster = cluster;
	         file -> pos = 0;

	         fs_fat_memcpy(dir_buffer, &(file -> desc), sizeof(t_directory));
	         if (!fs_fat_write_sector(buffer_last_read_sector)) return 0;

	         fs_fat_setentry(cluster, EOC_MARK); /* place eoc mark */
	         
	         /*printf("FILE CREATED\n"); /* DEBUG */

	         return file;
			}

			dir_buffer++;
		}
	}
	while (fs_fat_findnextdirentry(&tdir, 1));
   
	return 0;
}

t_file *fs_fat_fopen(u8 *name, u8 mode, t_file *handle)
{
   int i, ok;
   t_directory dir;
   t_directory *tdir = &dir;
	
	if (mode == 'r')
	{
		fs_fat_findfirst(&tdir);
   	while (tdir)
   	{
			ok = 1;
      	for (i = 0; i < 11; i++)
      	{
         	if (tdir -> DIR_Name[i] != name[i])
         	{
            	ok = 0;
				}
			}
		
			if (ok)
			{
         	fs_fat_memcpy(&(handle -> desc), &dir, sizeof(t_directory));
         
				handle -> pos = 0;
				fs_fat_fseek(handle, 0);
			
				return handle;
			}

			fs_fat_findnext(&tdir);
		}
	}
	else if (mode == 'w')
	{
		/* TODO: init *tdir fields */
		tdir = &(handle -> desc);
		for (i = 0; i < 11; i++) tdir -> DIR_Name[i] = name[i];
		tdir -> DIR_FileSize = 0;
		tdir -> DIR_Attr = 0;
		return fs_fat_create_file(handle);
	}
	
	return 0;
}

u32 fs_fat_fread(t_file *handle, void *dest_buffer, u32 length)
{
	u32 sec, cluster, cluster_cnt, copy_start, copy_length;
	u32 bytes_read = 0;
	u32 cluster_offset;
	
	if (handle == 0) return 0;
	
	/* if operation exceeds file length, correct it */
	if (handle -> pos + length > fs_fat_fsize(handle))
	{
		length = fs_fat_fsize(handle) - handle -> pos;
	}
	
	cluster = handle -> cluster;
	
	/* walk along clusters */
	while (bytes_read < length)
	{
		sec = fs_fat_cluster_to_sec(cluster) +
			(!bytes_read ? (handle -> pos / bytes_per_sec) % sec_per_clus : 0);
		
      cluster_offset =
			(!bytes_read ? (handle -> pos % (sec_per_clus * bytes_per_sec)) : 0);

		do
		{
			if (bytes_read == 0)
			{
            copy_start = handle -> pos % bytes_per_sec;
            copy_length = bytes_per_sec - copy_start;
            if (copy_length > length) copy_length = length;
			}
			else if (length - bytes_read < bytes_per_sec)
			{
				copy_start = 0;
				copy_length = (length + handle -> pos) % bytes_per_sec;
			}
			else
			{
				copy_start = 0;
				copy_length = bytes_per_sec;
			}

			fs_fat_read_sector_st(sec);
			fs_fat_memcpy(
				((u8*)dest_buffer) + bytes_read,
				((u8*)buffer) + copy_start,
				copy_length);

			bytes_read += copy_length;
			cluster_offset += copy_length;
			
   		sec++;
		}
		while ((bytes_read < length) && ((sec - first_data_sec) % sec_per_clus > 0));
		
		if ((cluster_offset >= (sec_per_clus * bytes_per_sec)) &&
			!fs_fat_iseoc(cluster)) /* TODO: ennek a feltételnek nem itt kéne lennie */
		{
         cluster = fs_fat_getentry(cluster);
		}
		
		if (bytes_read >= length) break;
	}
	
	handle -> pos += bytes_read;
	handle -> cluster = cluster;
	
	return bytes_read;
}

u32 fs_fat_fwrite(t_file *handle, void *source_buffer, u32 length)
{
	u32 sec, cluster, cluster_cnt, copy_start, copy_length;
	u32 bytes_written = 0;
	u32 cluster_offset;
	u32 new_cluster;
	
	if (handle == 0) return 0;
	
	/* if file size would increase after write operation, resize it's FAT chain*/
	if (handle -> pos + length > fs_fat_fsize(handle))
	{
      fs_fat_fresize(handle, handle -> pos + length);
	}
	
	/*printf("WAIT...\n"); getch(); /* DEBUG */

	cluster = handle -> cluster;

	/* walk along clusters */
	while (bytes_written < length)
	{
		sec = fs_fat_cluster_to_sec(cluster) +
			(!bytes_written ? (handle -> pos / bytes_per_sec) % sec_per_clus : 0);

      cluster_offset =
			(!bytes_written ? (handle -> pos % (sec_per_clus * bytes_per_sec)) : 0);

		do
		{
			if (bytes_written == 0)
			{
            fs_fat_read_sector_st(sec);
            copy_start = handle -> pos % bytes_per_sec;
            copy_length = bytes_per_sec - copy_start;
            if (copy_length > length) copy_length = length;
			}
			else if (length - bytes_written < bytes_per_sec)
			{
            fs_fat_read_sector_st(sec);
            copy_start = 0;
				copy_length = (length + handle -> pos) % bytes_per_sec;
			}
			else
			{
				copy_start = 0;
				copy_length = bytes_per_sec;
			}

			fs_fat_memcpy(
				((u8*)buffer) + copy_start,
				((u8*)source_buffer) + bytes_written,
				copy_length);
				
         fs_fat_write_sector(sec);
         /*printf("WRITTEN CLUSTER = %d\n", cluster); /* DEBUG */

			bytes_written += copy_length;
			cluster_offset += copy_length;
			
   		sec++;
		}
		while ((bytes_written < length) && ((sec - first_data_sec) % sec_per_clus > 0));
		
		/*printf("- NEXT CLUSTER -\n"); /* DEBUG */
		
		if (cluster_offset >= (sec_per_clus * bytes_per_sec))
		{
			new_cluster = fs_fat_getentry(cluster);
         if (fs_fat_iseoc(new_cluster))
			{
				if (fs_fat_fresize(handle, handle -> pos + length + 1))
				{
					new_cluster = fs_fat_getentry(cluster);
				}
				else
				{
					/*printf("DISK FULL\n"); /* DEBUG */
					/* probably disk is full... */
					/* error code is already set by fs_fat_fresize */
                  break;
				}
			}
			cluster = fs_fat_getentry(cluster);
		}
		
		if (bytes_written >= length) break;
	}

	handle -> pos += bytes_written;
	handle -> cluster = cluster;

	return bytes_written;
}

s8 fs_fat_fseek(t_file *handle, u32 position)
{
	u32 i, cluster, jmp_clus;
	
	if (position > fs_fat_fsize(handle)) /* do not jump out of file */
	{
		position = fs_fat_fsize(handle);
	}

	jmp_clus = position / (bytes_per_sec * sec_per_clus);
	
	cluster = fs_fat_get_first_cluster(&(handle -> desc));
		
	/* walk over the number of clusters we calculated above */
	for (i = 0; i < jmp_clus; i++)
	{
		cluster = fs_fat_getentry(cluster);
	}
	
	/* store seek position and current cluster */
	handle -> pos = position;
	handle -> cluster = cluster;

	return 1;
}

u32 fs_fat_fsize(t_file *handle)
{
	return (handle -> desc).DIR_FileSize;
}

s8 fs_fat_ferase(u8 *name)
{
   int i, j, ok;
   u32 cluster;      /* will store the first cluster of the file */
	u32 next_cluster; /* needed for clearing the file's FAT chain */
   t_directory dir;
   t_directory *tdir = &dir;

	fs_fat_findfirst(&tdir);
   while (tdir)
   {
		ok = 1;
      for (i = 0; i < 11; i++)
      {
         if (tdir -> DIR_Name[i] != name[i])
         {
            ok = 0;
			}
		}

		if (ok) /* found the file */
		{
			/* t_directory is still in the buffer now, modify it and write out */
			t_directory *dir_buffer = (t_directory *) buffer;

			/* find the entry in the memory buffer */
			for (j = 0; j < bytes_per_sec / sizeof(t_directory); j++)
			{
            ok = 1;
      		for (i = 0; i < 11; i++)
      		{
         		if (dir_buffer[j].DIR_Name[i] != name[i])
         		{
            		ok = 0;
					}
				}

				if (ok) /* found the entry */
				{
               dir_buffer[j].DIR_Name[0] = 0xe5; /* mark as deleted */

					if (!fs_fat_write_sector(buffer_last_read_sector)) return 0;
					
					/* store it's first cluster */
					cluster = fs_fat_get_first_cluster(&(dir_buffer[j]));
					
					/*	(((u32) (dir_buffer[j].DIR_FstClusHI)) << 16) |
						dir_buffer[j].DIR_FstClusLO; */

               break;
				}
			}

         /* clear fat chain */
         next_cluster = cluster;
   		do
			{
            cluster = next_cluster;
      		
      		if (!fs_fat_iseoc(cluster))
      		{
					next_cluster = fs_fat_getentry(cluster); /* next cluster */
					fs_fat_setentry(cluster, 0);   /* zero out current cluster entry */
				}
				else next_cluster = 0; /* use as loop condition */
				
			} while (next_cluster != 0);

			return 1;
		}

		fs_fat_findnext(&tdir);
	}

   return 0;
}

s8 fs_fat_fresize(t_file *handle, u32 size)
{
	u32 next, i, j, ok, new_entry, new_size, disk_full;
   
	u32 entry = fs_fat_get_first_cluster(&(handle -> desc));
	u32 bytes_per_clus = sec_per_clus * bytes_per_sec;
	
	t_directory dir;
   t_directory *tdir = &dir;
	
	/* calculate current and desired file size in clusters */
	u32 current_size_in_clus =
		(fs_fat_fsize(handle) + bytes_per_clus - 1) / bytes_per_clus;
	u32 desired_size_in_clus = (size + bytes_per_clus - 1) / bytes_per_clus;
	
	/* although file size is 0, we already have a FAT entry */
	if (current_size_in_clus == 0) current_size_in_clus = 1;
	
	disk_full = 0;
	
	if (current_size_in_clus < desired_size_in_clus) /* grow file size */
	{
		/* after this loop entry will point to the EOC entry */
		while (!fs_fat_iseoc(next = fs_fat_getentry(entry)))
		{
			entry = next;
		}
		
		new_size = handle -> desc.DIR_FileSize; /* count from current size */
		
		/* create FAT chain */
		for (i = 0; i < (desired_size_in_clus - current_size_in_clus); i++)
		{
			new_entry = fs_fat_getfreeentry();
			if (new_entry != 0)
			{
         	fs_fat_setentry(entry, new_entry);
         	fs_fat_setentry(new_entry, EOC_MARK); /* place EOC mark */
         	entry = new_entry;
			}
			else /* disk full */
			{
            fs_fat_setentry(entry, EOC_MARK); /* place EOC mark */
            size = new_size; /* size'll be the position we've reached so far */
            disk_full = 1; /* disk is full */
            break; /* error */
			}
			
			new_size += bytes_per_sec * sec_per_clus; /* inc. size by 1 cluster */
		}
	}
	else if (current_size_in_clus > desired_size_in_clus) /* shrink file size */
	{
		/* TODO */
	}
	
	handle -> desc.DIR_FileSize = size; /* refresh file size */
	
	/* find corresponding directory entry */
	fs_fat_findfirst(&tdir);
	while (tdir)
	{
	   /* if empty t_directory is found in the buffer, modify and write out */
		tdir = (t_directory *) buffer;

		for (j = 0; j < bytes_per_sec / sizeof(t_directory); j++)
		{
	      ok = 1;
	      for (i = 0; i < 11; i++)
	      {
	         if (tdir -> DIR_Name[i] != handle -> desc.DIR_Name[i])
	         {
	            ok = 0;
				}
			}

			if (ok)
			{
				fs_fat_memcpy(tdir, &(handle -> desc), sizeof(t_directory));
				fs_fat_write_sector(buffer_last_read_sector);
				break;
			}

			tdir++;
		}
		
	   fs_fat_findnext(&tdir); /* try next directory sector */
	}
	
	if (disk_full)
	{
      fs_fat_set_error_code(FS_FAT_ERR_DISKFULL); /* error code */
      return 0;
	}
	
	return 1; /* no error */
}

s8 fs_fat_fclose(t_file *handle)
{
	return 1;
}

s8 fs_fat_iseof(t_file *handle)
{
	return handle -> pos >= fs_fat_fsize(handle);
}

void fs_fat_set_error_code(s8 ec)
{
	error_code = ec;
}

s8 fs_fat_error_code()
{
	return error_code;
}


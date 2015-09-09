#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "fat.h"

FILE *file;
char file_name[30];
FILE *log_file;
FILE *html_file;

/* képfájl megnyitása */
void proc_m()
{
	printf("\nA kepfajl neve: ");
	gets(file_name);
	
	if (file != NULL)
	{
		fclose(file);
		file = NULL;
	}
	
	/* strcpy(file_name, "pendrive.img"); */
	
	file = fopen(file_name, "rb+"); /* binary mode is VERY important */
	
	if (file != NULL) fs_fat_init();
	else
	{
		printf("\nA megadott kepfajl nem talalhato!\n");
	}
}

/* képfájl jellemzõk */
void proc_j()
{
   printf("\nKepfajl jellemzoi:\n------------------\n");
   printf("Fajlnev          : %s\n", file_name);
   /*printf("OEM nev          : %s\n", );
   printf("Tipus            : FAT_%d\n", fs_fat_get_fat_type());
   printf("Fenntartott szek.: %d\n", resvd_sec_cnt);
   printf("Tablak szama     : %d\n", num_fats);
   printf("Klasztermeret    : %dkb\n", sec_per_clus * bytes_per_sec / 1024);
   printf("Bajt/szektor     : %d\n", bytes_per_sec);
   printf("Klaszterek szama : %d\n", cluster_cnt);
   printf("Szektorok szama  : %d\n", data_sec_cnt);
   printf("Szektor/Klaszter : %d\n", sec_per_clus);
   printf("Root dir szektor : %d\n", root_dir_sec);
   printf("Elso adatszektor : %d\n", first_data_sec);
   printf("FAT meret szekt. : %d\n", fat_size_sec);*/
}

/* fájlok listája */
void proc_l()
{
   t_directory *dir;
   int i = 0;
   long int cluster;
   char name[12];
   char attr[2] = { '.', 'X' };
   
   printf("\n");
   printf("  Nev           Meret      R H S V A   Kezdo kl.   Hossz\n");
   printf("--------------------------------------------------------\n");
   
   dir = malloc(sizeof(t_directory));
   i = 0;
   name[11] = 0;

   fs_fat_findfirst(&dir);

   while (dir)
   {
		memcpy(name, dir -> DIR_Name, 11);

      cluster = (((u32) (dir -> DIR_FstClusHI)) << 16) | dir -> DIR_FstClusLO;

      if ((dir -> DIR_Attr) & ATTR_DIRECTORY) printf("/");
		else printf(" ");

		printf(" %11s   %9d  %c %c %c %c %c   %8d\n",
			name,
			dir -> DIR_FileSize,
			attr[dir -> DIR_Attr & 0x01],
			attr[(dir -> DIR_Attr & 0x02) >> 1],
			attr[(dir -> DIR_Attr & 0x04) >> 2],
			attr[(dir -> DIR_Attr & 0x08) >> 3],
			attr[(dir -> DIR_Attr & 0x20) >> 5],
			cluster);

  		fs_fat_findnext(&dir);

		i++;
	}

	printf("\nOsszesen %d bejegyzes.\n", i);
	
	free(dir);
}

/* fájl tartalmának megjelenítése */
void proc_f()
{
   t_file f;
	char name[30];
	long pos;
	char *buf;
	unsigned int bytes_read;
	
   printf("\nA listazando fajl neve: ");
 	gets(name);
	
	if (fs_fat_fopen(name, 'r', &f))
	{
		buf = malloc(5001);

		printf("\n----[ %s ]----\n", name);
		
		while (!fs_fat_iseof(&f))
		{
			bytes_read = fs_fat_fread(&f, buf, 40);
			buf[bytes_read] = '\0';
			printf("%s", buf);
			/*if (f.pos % 2048 < 200)
			{
				printf("0x%x: %s\n", f.pos, buf);
				if (getch() == 'c') break;
			}*/
		}
		
		printf("\n----[ %s ]----\n", name);

		fs_fat_fclose(&f);
		free(buf);
	}
	else
	{
		printf("\nA megadott fajl nem talalhato.\n");
	}
}

/* új fájl létrehozása */
void proc_u()
{
   t_file f;
	char name[30];
	char *str;
	int i;

   printf("\nA letrehozando fajl neve: ");
	gets(name);

   if (fs_fat_fopen(name, 'w', &f))
   {
		printf("\nA fajl letrehozasa megtortent.\n\n----[ %s ]----\n", name);
		
		/* feltöltés */
		str = malloc(2000);
		gets(str);
		
		for (i = 0; i < 10; i++)
		{
			fs_fat_fwrite(&f, str, strlen(str));
		}
		
		fs_fat_fclose(&f);
		free(str);
		
		printf("----[ %s ]----\n", name);
	}
	else
	{
      printf("\nA megadott fajl nem hozhato letre.\n");
	}
}

/* fájl másolása */
void proc_c()
{
   t_file f,d;
	char name1[30];
	char name2[30];
	unsigned long int bytes_read, bytes_written;
	char *buf;
	int i;

   printf("\nA forras fajl neve: ");
 	gets(name1);
 	printf("A cel fajl neve: ");
 	gets(name2);
 	
	if (fs_fat_fopen(name1, 'r', &f) && fs_fat_fopen(name2, 'w', &d))
	{
		buf = malloc(2000);
		
		i = 0;
		
		while (!fs_fat_iseof(&f))
		{
			/*printf(".");*/
			bytes_read = fs_fat_fread(&f, buf, 2000);
			/*printf(".");*/
			bytes_written = fs_fat_fwrite(&d, buf, bytes_read);
			/*buf[bytes_read] = '\0';
			printf("COPY(%d/%d)[%s]\n", bytes_read, bytes_written, buf);*/
			i++;
		}
		
		printf("\nMuveletek szama: %d\n", i);

		fs_fat_fclose(&f);
		fs_fat_fclose(&d);
		free(buf);
	}
	else
	{
		printf("\nA megadott fajlok valamelyike nem talalhato.\n");
	}
}

/* fájl átméretezése */
void proc_r()
{
   t_file f;
	char name[30];
	long pos;
	char *buf;

   printf("\nAz atmeretezendo fajl neve: ");
 	gets(name);

	if (fs_fat_fopen(name, 'r', &f))
	{
		fs_fat_fresize(&f, 2 * fs_fat_fsize(&f));
		fs_fat_fclose(&f);
	}
	else
	{
		printf("\nA megadott fajl nem talalhato.\n");
	}
}

/* fájl törlése */
void proc_t()
{
   t_file f;
	char name[30];
	char *buf;

   printf("\nA torlendo fajl neve: ");
	gets(name);
	
   if (fs_fat_ferase(name))
   {
		printf("\nA fajl torlese sikeres.\n");
	}
	else
	{
      printf("\nA megadott fajl nem talalhato.\n");
	}
}

void html(char *code)
{
   fprintf(html_file, "%s", code);
}

/* html generálás */
void proc_h()
{
	/*int i, j;
	int sec = 0;
	int resvd_sec_cnt2 = resvd_sec_cnt - 1;
	
	if ((html_file = fopen("output.htm", "w")) != NULL)
	{
		html("<html>\n");
		html("  <head>\n");
		html("  <meta http-equiv=\"content-type\" content=\"text/html; charset=iso-8859-2\">\n");
    	html("  <style type=\"text/css\" media=\"screen\">@import \"style.css\";</style>\n");
		fprintf(html_file, "  <title>FAT (%s)</title>\n", file_name);
		html("<head>\n\n");
		html("<body>\n");
		
		html("<div class=\"sec_boot\">boot sector</div>\n");
		
		for (i = 0; i < resvd_sec_cnt2; i++)
		{
			if (resvd_sec_cnt2 == 1)
				html("<div class=\"sec_resvd\">reserved sector</div>\n");
         else fprintf(html_file, "<div class=\"sec_resvd\">reserved sector #%d</div>\n", i + 1);
		}
		
		html("<br class=\"clear\">\n");
		
		for (i = 0; i < num_fats; i++)
		{
			if (num_fats == 1)
				html("<div class=\"group_fat\">FAT table<br>\n");
         else fprintf(html_file, "<div class=\"group_fat\">FAT table #%d<br>\n", i + 1);
         
         for (j = 0; j < fat_size_sec; j++)
			{
				if (fat_size_sec == 1)
					html("<div class=\"sec_fat\">FAT sector</div>\n");
         	else fprintf(html_file, "<div class=\"sec_fat\">FAT sector #%d</div>\n", j + 1);
			}
         
         html("\n</div>\n");
		}
		
		html("<br class=\"clear\">\n");
		
		if (fat_type != 32)
		{
			int root_size = first_data_sec - root_dir_sec;
			
         html("<div class=\"group_root\">root directory region<br>\n");
         
         for (j = 0; j < root_size; j++)
			{
				if (root_size == 1)
					html("<div class=\"sec_root\">root sector</div>\n");
         	else fprintf(html_file, "<div class=\"sec_root\">root sector #%d</div>\n", j + 1);
			}

         html("\n</div>\n");
		}
		
		for (i = 0; i < cluster_cnt; i++)
		{
			if (i == 0 || i == cluster_cnt - 1)
			{
				if (cluster_cnt == 1)
					html("<div class=\"group_cluster\">data cluster<br>\n");
         	else fprintf(html_file, "<div class=\"group_cluster\">data cluster #%d<br>\n", i + 1);

         	for (j = 0; j < sec_per_clus; j++)
				{
					if (sec_per_clus == 1)
						html("<div class=\"sec_cluster\">data sector</div>\n");
         		else fprintf(html_file, "<div class=\"sec_cluster\">data sector #%d</div>\n", j + 1);
				}

         	html("\n</div>\n");
			}
		}

		html("<body>\n\n");
		html("</html>\n");
		
		fclose(html_file);
	}
	else
	{
		printf("\nAz output.htm fajl nem hozhato letre.\n");
	}*/
}

/* belépés könyvtárba */
void proc_o()
{
	char name[30];
	
   printf("\nA konyvtar neve: ");
 	gets(name);
	
	if (!fs_fat_chdir(name))
	{
		printf("\nA megadott konyvtar nem talalhato.\n");
	}
}

s8 read_sector_to_buffer_sample(u32 lba, void * buffer)
{
	fseek(file, 512 * lba, SEEK_SET);
   fread(buffer, 1, 512, file);
   printf("#READ\n"); /* DEBUG */
   return 1;
}

s8 write_sector_from_buffer_sample(u32 lba, void * buffer)
{
	/*printf("\n--> DEBUG: write_sector_from_buffer(%d);\n", lba);*/
	if (512 * lba <= filelength(file))
	{
   	   fseek(file, 512 * lba, SEEK_SET);
   	   fwrite(buffer, 1, 512, file);
   	   printf("#WRITE\n"); /* DEBUG */
   	   return 1;
	}
	else
	{
		printf("### Vegzetes hiba! Iras a kepfajlon kivulre! ###\n");
		return 0;
	}
}

u8 sector_cache[512];
u32 sector_cache_current = -1;
u8 sector_dirty = 0;

s8 write_sector_from_buffer(u32 lba, void * buffer)
{
    sector_dirty = 1; /* sector in buffer is now 'dirty' */
	/* if we want to write another sector and the one in the buffer is dirty */
    if ((lba != sector_cache_current) && sector_dirty)
	{  
       /* physical write with error checking */
       if (512 * sector_cache_current <= filelength(file))
	   {
   	      fseek(file, 512 * sector_cache_current, SEEK_SET);
   	      if (fwrite(sector_cache, 1, 512, file) != 512) return 0; /* error */
       }
	   else
	   {
		   printf("ERROR! ATTEMPT TO WRITE OUTSIDE OF IMAGE FILE.\n");
		   return 0; /* error */
	   }
    }
    /* copy bytes to write into sector buffer */
    if (buffer != 0)
    {
       memcpy(sector_cache, buffer, 512);
       /* store lba address as current address */
       sector_cache_current = lba;
    }
    return 1; /* no error */
}

s8 read_sector_to_buffer(u32 lba, void * buffer)
{
   if (lba != sector_cache_current)
   {
      /* first write sector out if dirty */
      if (sector_dirty)
      {
         write_sector_from_buffer(-1, 0);
         sector_dirty = 0;
      }
      /* physical sector read */
      fseek(file, 512 * lba, SEEK_SET);
      if (fread(sector_cache, 1, 512, file) != 512) return 0; /* error */
      /* store lba address as current address */
      sector_cache_current = lba;
   }
   
   /* copy bytes read to destination buffer */
   memcpy(buffer, sector_cache, 512);
   
   return 1; /* no error */
}

s8 write_sector_flush()
{
   if (sector_dirty)
   {
      write_sector_from_buffer(-1, 0);
   }
}

int main()
{
   char ch;
   
   fs_fat_read_sector_to_buffer = read_sector_to_buffer;
   fs_fat_write_sector_from_buffer = write_sector_from_buffer;
   fs_fat_memcpy = (void *) memcpy;
   
   buffer = malloc(1024);
   fat_buffer = malloc(1024);
   
   file = NULL;
   
   do
   {
		printf("\n+----------------------------+\n");
   	printf("| M - Kepfajl megnyitasa     |\n");
   	if (file != NULL)
   	{
		printf("| J - Jellemzok              |\n");
   		printf("| L - Listazas               |\n");
   		printf("| F - Fajl listazas          |\n");
   		printf("| U - Uj fajl                |\n");
   		printf("| C - Masol                  |\n");
   		printf("| R - Fajl atmeretezes       |\n");
   		printf("| T - Fajl torles            |\n");
   		printf("| O - Konyvtarvaltas         |\n");
   		printf("| H - HTML generalas         |\n");
     }
   	printf("| K - Kilepes                |\n");
   	printf("+----------------------------+\n");
   	printf("\nMit szeretne tenni? ");
   	
   	ch = getch();
   	if (ch >= 'a' && ch <= 'z') ch &= 0xdf;
   	printf("%c\n", ch);
   	
		switch (ch)
		{
			case 'M' : proc_m(); break;
			case 'J' : if (file != NULL) proc_j(); break;
			case 'L' : if (file != NULL) proc_l(); break;
			case 'F' : if (file != NULL) proc_f(); break;
			case 'U' : if (file != NULL) proc_u(); break;
			case 'C' : if (file != NULL) proc_c(); break;
			case 'R' : if (file != NULL) proc_r(); break;
			case 'T' : if (file != NULL) proc_t(); break;
			case 'O' : if (file != NULL) proc_o(); break;
			case 'H' : if (file != NULL) proc_h(); break;
		}
   
	} while (ch != 'K');
	
	write_sector_flush();
	
	if (file != NULL) fclose(file);
	
	free(buffer);
	free(fat_buffer);
}

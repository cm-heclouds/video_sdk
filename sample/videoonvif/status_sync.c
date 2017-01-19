#include <stdio.h>
#include <stdarg.h>
#include "ont/platform.h"
#include "status_sync.h"


#define CHANNELINFO_SYNC_FILE "channelsync"



int  status_synced(uint32_t nchn)
{
	uint32_t chnnum;
	size_t rsize;
	long fsize = 0;
	FILE *hfile = fopen(CHANNELINFO_SYNC_FILE, "r+");
	if (hfile)
	{
		fseek(hfile, 0, SEEK_END);
		fsize = ftell(hfile);
		if (fsize >= sizeof(chnnum))
		{
			fseek(hfile, 0, SEEK_SET);
			rsize = fread(&chnnum, sizeof(chnnum), 1, hfile);
			if (nchn == chnnum)
			{
				return 1;
			}
			else
			{
				fwrite(&nchn, sizeof(nchn), 1, hfile);
				return 0;
			}
		}
		else
		{
			fwrite(&nchn, sizeof(nchn), 1, hfile);
		}
	}
	else
	{
		hfile = fopen(CHANNELINFO_SYNC_FILE, "w+");
		fwrite(&nchn, sizeof(nchn), 1, hfile);
		return 0;
	}

	fclose(hfile);
	return 0;
}

int  file_synced(file_record *filerecord)
{
	file_record record;
	size_t rsize;
	int i = 0;
	FILE *hfile = fopen(CHANNELINFO_SYNC_FILE, "r+");
	if (hfile)
	{
		while (1)
		{
			fseek(hfile, sizeof(uint32_t) + i*sizeof(file_record), SEEK_SET);
			rsize = fread(&record, sizeof(record), 1, hfile);
			if (rsize<=0)
			{
				break;
			}

			if (memcmp(filerecord, &record, sizeof(file_record)) == 0)
			{
				return 1;
			}
			else
			{
				i++;
			}
		}
	}
	else
	{
		return -1;
	}
	fseek(hfile, sizeof(uint32_t) + i*sizeof(file_record), SEEK_SET);
	fwrite(filerecord, sizeof(file_record), 1, hfile);
	fclose(hfile);
	return 0;
}
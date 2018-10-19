#include <stdio.h>
#include <stdarg.h>

#include "platform.h"

#define CHANNELINFO_SYNC_FILE "channelsync"

int status_synced(uint32_t nchn)
{
	uint32_t chnnum;
#if 0 /* rsize set but not used */
	size_t rsize = 0;
#endif
	long fsize = 0;
	FILE *hfile = fopen(CHANNELINFO_SYNC_FILE, "r+");
	if (hfile) {
		fseek(hfile, 0, SEEK_END);
		fsize = ftell(hfile);
		if (fsize >= sizeof(chnnum)) {
			fseek(hfile, 0, SEEK_SET);
			/* resize set but not used */
			/* rsize = */(void)fread(&chnnum, sizeof(chnnum), 1, hfile);
			if (nchn == chnnum) {
				return 1;
			} else {
				fwrite(&nchn, sizeof(nchn), 1, hfile);
				return 0;
			}
		} else {
			fwrite(&nchn, sizeof(nchn), 1, hfile);
		}
	} else {
		hfile = fopen(CHANNELINFO_SYNC_FILE, "w+");
		fwrite(&nchn, sizeof(nchn), 1, hfile);
		return 0;
	}

	fclose(hfile);
	return 0;
}

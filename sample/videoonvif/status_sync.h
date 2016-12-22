#ifndef _STATUS_SYNC_H_
#define _STATUS_SYNC_H_

typedef struct file_record_tag
{
	uint32_t chnnum;
	char begintime[20];
	char endtime[20];
}file_record;

int  status_synced(uint32_t chnnum);
int  file_synced(file_record *filerecord);

#endif
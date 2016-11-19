#ifndef MGMT_H
#define MGMT_H

#include <stdlib.h>
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "expr.h"


typedef struct PageFrame
{
	int pageFrameNum;
	int pageNum;
	BM_PageHandle *bph;
	bool is_dirty;
	int pageFrameCount;
	int freqCount;
	int fixcounts;
	struct PageFrame *nxtFrame;
}PageFrame;

//structure for storing info of BufferPool
typedef struct BM_BufferPoolInfo
{	
	int numReadIO;
	int numWriteIO;
	SM_FileHandle *fh;
	PageFrame *node;
	PageFrame *search;
	PageFrame *currFrame;	
	PageFrame *strtFrame;
}BM_BufferPoolInfo;

//implementation of Hash map to check Primary Key
typedef struct Record_PKey
{
    char *data;
    struct Record_PKey *nextkey;
} Record_PKey;



//structure for records being created
typedef struct RM_RecordPoolInfo
{
    BM_BufferPool *bm;
    int *emptyPages;
    Record_PKey *arr_keys;
    Record_PKey *node;
    Record_PKey *curr;
} RM_RecordPoolInfo;

//structure for scanning of pool
typedef struct RM_ScanPool
{
    Expr *condition;
    int page_current;
    Record *currRecord;
} RM_ScanPool;

#endif

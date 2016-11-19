#include "mgmt.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "buffer_mgr_stat.h"
#include "dt.h"
#include "dberror.h"
#include "stdio.h"
#include "stdlib.h"
#include "pthread.h"

#define FRAME_SIZE 100

/*Initializing the mutex variables for thread safety*/
static pthread_mutex_t mutex_init = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_unpinPage = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_pinPage = PTHREAD_MUTEX_INITIALIZER;

//

////////////////////////////////////////////////////////////////
//Initialize BufferPool with BufferPool info
////////////////////////////////////////////////////////////////
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData)
{
	//BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;

	//check if BufferPool is Null
	if(bm->mgmtData == NULL)
	{
	
		pthread_mutex_lock(&mutex_init); // Lock mutex while returning
		BM_BufferPoolInfo *pool = (BM_BufferPoolInfo *)malloc(sizeof(BM_BufferPoolInfo));
		pool->fh = (SM_FileHandle *)malloc(sizeof(SM_FileHandle));

		if(pool->fh == NULL)
		{
			free(pool->fh);
			pool->fh = NULL; //assign null to pool
			free(pool);
			pool = NULL;
			pthread_mutex_unlock(&mutex_init);// Unlock mutex while returning
			return RC_FILE_HANDLE_NOT_INIT;
		}
		
		if(openPageFile(pageFileName, pool->fh) == RC_FILE_NOT_FOUND)
		{
			pthread_mutex_unlock(&mutex_init);// Unlock mutex while returning
			return RC_FILE_NOT_FOUND;
		}
		pool->strtFrame = NULL;
		pool->currFrame = NULL;
		pool->node = NULL;
		pool->search = NULL;
		pool->numReadIO = 0;
		pool->numWriteIO = 0;
		bm->pageFile = pageFileName;
		bm->numPages = numPages;
		bm->strategy = strategy;
		bm->mgmtData = pool;
		pthread_mutex_unlock(&mutex_init); // Unlock mutex while returning
		return RC_OK;
	}
	else
	{
		return RC_BUFFER_POOL_ALREADY_INIT;
	}
}

////////////////////////////////////////////////////////////////
//Shut down BufferPool
////////////////////////////////////////////////////////////////
RC shutdownBufferPool(BM_BufferPool *const bm)
{
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;

	//check if the BufferPool is initialized
	if(bm->mgmtData != NULL)
	{
		//if start frame of the pool is Null, free the pool
		if(pool->strtFrame == NULL)
		{
			
			pool->fh = NULL;
			free(pool->fh);
			
			bm->mgmtData = NULL;
			free(bm->mgmtData);
			free(bm);
			return RC_OK;
		}
		pool->currFrame = pool->strtFrame;
		while(pool->currFrame != NULL)
		{
			pool->currFrame->fixcounts = 0;
			pool->currFrame = pool->currFrame->nxtFrame;
		}
		int status = forceFlushPool(bm);

		//Release the memory	
		pool->fh = NULL;
		free(pool->fh);
		bm->mgmtData = NULL;
		free(bm->mgmtData);
		free(bm);

		return RC_OK;
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;
	}
}
////////////////////////////////////////////////////////////////
//Flush the BufferPool
////////////////////////////////////////////////////////////////
RC forceFlushPool(BM_BufferPool *const bm)
{
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;

	//check if the BufferPool is initialized
	if(pool != NULL)
	{
		pool->node=pool->strtFrame;
		if(pool->node != NULL)
		{
			while(pool->node != NULL)
			{
				//check if the page is dirty
				if(pool->node->is_dirty)
				{
					if(pool->node->fixcounts == 0)
					{
						if(writeBlock(pool->node->pageNum,pool->fh,pool->node->bph->data) == RC_OK)
						{	
							pool->node->is_dirty=0; //change the dirty page
							pool->numWriteIO +=1;
						}
					}
				}
				pool->node=pool->node->nxtFrame;
			}
			return RC_OK;
		}
		else
		{
			return RC_BUFFER_POOL_NULL;
		}
	}
	else
	{	
		return RC_BUFFER_POOL_NOT_INIT;		
	}
	
}

////////////////////////////////////////////////////////////////
//Find the position of particular page
////////////////////////////////////////////////////////////////
PageFrame *findPage(BM_BufferPoolInfo *pool, PageNumber pageNumber)
{
	pool->node=pool->strtFrame;
	while(pool->node != NULL)
	{	
		if(pool->node->pageNum == pageNumber)
		{
			break;
		}
		pool->node=pool->node->nxtFrame;
	}
	return pool->node;
}

////////////////////////////////////////////////////////////////
//Mark the page of the pool as dirty
////////////////////////////////////////////////////////////////
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;

	//check if the BufferPool is initialized	
	if(bm->mgmtData != NULL)
	{
		pool->search=(PageFrame *)findPage(bm->mgmtData,page->pageNum);
		if(pool->search != NULL)
		{
			pool->search->is_dirty=1; //mark the page as dirty
			return RC_OK;		
		}
			//return RC_BUFFER_POOL_ERROR_IN_MARKDIRTY;
			//return RC_OK;		
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;
	}	
}
////////////////////////////////////////////////////////////////
//Detach the page from the pool
////////////////////////////////////////////////////////////////
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;	

	//check if the BufferPool is initialized
	if(pool != NULL)
	{
		pthread_mutex_lock(&mutex_unpinPage);
		pool->search=(PageFrame *)findPage(bm->mgmtData,page->pageNum); //search the page

		//check if the search is null
		if(pool->search != NULL)
		{
			pool->search->fixcounts-=1;
			pthread_mutex_unlock(&mutex_unpinPage);
			return RC_OK;		
		}	
		pthread_mutex_unlock(&mutex_unpinPage);
		//return RC_OK;		
		//return RC_BUFFER_POOL_ERROR_IN_UNPINPAGE;			
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;	
	}	
			
}
////////////////////////////////////////////////////////////////
//Write dirty page to disk before replaing
////////////////////////////////////////////////////////////////
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;	
	if(bm->mgmtData != NULL)
	{
		pool->search=(PageFrame *)findPage(bm->mgmtData,page->pageNum); //search the page
		//printf("in");
		//check if the search is null
		if(pool->search != NULL)
		{
			//printf("ds");
			if(writeBlock(pool->search->pageNum,pool->fh,pool->search->bph->data) == RC_OK)
			{	
			//	printf("dssd");
				pool->search->is_dirty=0;
				pool->numWriteIO +=1;
				return RC_OK;	
			}	
		}
		else
		{
			return RC_OK;
		}			
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;	
	}	
}

////////////////////////////////////////////////////////////////
//check and get the empty frame of the BufferPool
////////////////////////////////////////////////////////////////
int GetEmptyFrame(BM_BufferPool *bm)
{
	int i = 0;
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;	
	pool->node = pool->strtFrame;
	while(pool->node != NULL)
	{
		if(pool->node->pageFrameNum != i)
		{ 
			return i; 
		}	
		i++;
		pool->node=pool->node->nxtFrame;
	}
	
	if(i < bm->numPages)
	{
		return i;
	}	
	else
	{
		return -1;
	}
}

////////////////////////////////////////////////////////////////
//Return the size of the BufferPool
////////////////////////////////////////////////////////////////
int PoolSize(BM_BufferPoolInfo *pool)
{
	int i = 0;
	pool->node = pool->strtFrame; 
	while(pool->node != NULL)
	{
		i++;
		pool->node=pool->node->nxtFrame;
	}
	return i;
}

////////////////////////////////////////////////////////////////
//Change the count value
////////////////////////////////////////////////////////////////
void ChangeCount(BM_BufferPoolInfo *pool)
{
	pool->node = pool->strtFrame; 
	while(pool->node != NULL)
	{
		pool->node->pageFrameCount = pool->node->pageFrameCount + 1;
		pool->node = pool->node->nxtFrame;
	}
}

////////////////////////////////////////////////////////////////
//Find and return the count of buffer
////////////////////////////////////////////////////////////////
PageFrame *findCount(BM_BufferPoolInfo *pool)
{
	int high = 0;
	pool->node = pool->strtFrame;
	pool->search = pool->strtFrame;
	while(pool->node != NULL)
	{
		if(pool->node->pageFrameCount > high)
		{
			if(pool->node->fixcounts == 0)
			{
				high = pool->node->pageFrameCount;
				pool->search = pool->node;
			}
		}
		pool->node = pool->node->nxtFrame;
	}
	return pool->search;
}



////////////////////////////////////////////////////////////////
//Replace the page according to strategy
////////////////////////////////////////////////////////////////
int replacementStrategy(BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum1)
{
	int a;
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;	
	pool->search = findCount(bm->mgmtData);
	if(pool->search->is_dirty == 1)
	{
		if(writeBlock(pool->search->pageNum,pool->fh,pool->search->bph->data) == RC_OK)
		{
			pool->search->is_dirty = 0;
			pool->numWriteIO += 1;
		}
	}
	ChangeCount(bm->mgmtData);
	int status=readBlock(pageNum1,pool->fh,pool->search->bph->data);
	if(status == RC_OK)
	{
		pool->search->bph->pageNum = pageNum1;
		pool->search->pageNum = pageNum1;
		pool->search->is_dirty = 0;
		pool->search->pageFrameCount = 1;
		pool->search->freqCount = 1;
		pool->search->fixcounts = 1;
		page->data = pool->search->bph->data;
		page->pageNum = pageNum1;
		return RC_OK;
	}
	return status;
}

////////////////////////////////////////////////////////////////
//Get and return the highest count of the BufferPool
////////////////////////////////////////////////////////////////
int getHighestCount(BM_BufferPoolInfo *pool)
{
	int high = 0;
	pool->node = pool->strtFrame;
	pool->search = pool->strtFrame;
	while(pool->node != NULL)
	{
		if(pool->node->pageFrameCount > high)
		{
			if(pool->node->fixcounts == 0)
			{
				high = pool->node->pageFrameCount;
				pool->search = pool->node;
			}
		}
		pool->node = pool->node->nxtFrame;
	}
	return pool->search->pageFrameCount;
}

////////////////////////////////////////////////////////////////
//Return lowest frequency count for LFU strategy
////////////////////////////////////////////////////////////////
PageFrame *findLowFreqCnt(BM_BufferPoolInfo *pool)
{
	int low = 99999;
	int highCount = getHighestCount(pool);
	pool->node = pool->strtFrame;
	pool->search = pool->strtFrame;
	while(pool->node != NULL)
	{
		if(pool->node->freqCount <= low)
		{
			if(pool->node->fixcounts == 0 && pool->node->pageFrameCount <= highCount && pool->node->pageFrameCount != 1)
			{
				low = pool->node->freqCount;
				pool->search = pool->node;
			}
		}
		pool->node = pool->node->nxtFrame;
	}
	return pool->search;
}
////////////////////////////////////////////////////////////////
//Implement LFU strategy
////////////////////////////////////////////////////////////////
int LFU(BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum)
{
	int status;
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;	
	pool->search = findLowFreqCnt(bm->mgmtData);
	if(pool->search->is_dirty == 1)
	{
		status = writeBlock(pool->search->pageNum,pool->fh,pool->search->bph->data);
		if (status == RC_OK)
		{
			pool->search->is_dirty = 0;
			pool->numWriteIO += 1;
		}
	}
	ChangeCount(bm->mgmtData);
	status = readBlock(pageNum,pool->fh,pool->search->bph->data);
	if(status == RC_OK)
	{
		pool->search->bph->pageNum = pageNum;
		pool->search->pageNum = pageNum;
		pool->search->is_dirty = 0;
		pool->search->pageFrameCount = 1;
		pool->search->freqCount = 1;
		pool->search->fixcounts = 1;
		page->data = pool->search->bph->data;
		page->pageNum = pageNum;
		return RC_OK;
	}
	return status;
}

////////////////////////////////////////////////////////////////
//Pin Page in BufferPool
////////////////////////////////////////////////////////////////
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum1)
{
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;	
	if(bm->mgmtData != NULL)
	{	
		pthread_mutex_lock(&mutex_pinPage);//Lock mutex while returning
		if(pageNum1 >= pool->fh->totalNumPages)
		{
			//printf("Creating missing page %i\n", pageNum1);
			int a = ensureCapacity(pageNum1 + 1, ((BM_BufferPoolInfo *)bm->mgmtData)->fh);
		}
		pool->search = findPage(bm->mgmtData, pageNum1);
		pool->node = pool->strtFrame;
		while(pool->search)
		{
			if(pool->search->pageNum == pageNum1)
			{
				pool->node = ((BM_BufferPoolInfo *)bm->mgmtData)->search;
				break;
			}
			pool->search = pool->search->nxtFrame;
		}
		if(pool->search != pool->node || pool->search == 0)
		{
			int empty = GetEmptyFrame(bm);
			if (empty != -1)
			{
				if(PoolSize(bm->mgmtData) == 0)
				{
					pool->strtFrame = (PageFrame *)malloc(sizeof(PageFrame));
					pool->strtFrame->bph = MAKE_PAGE_HANDLE();
					pool->strtFrame->bph->data = (char *) malloc(PAGE_SIZE);
					int status = readBlock(pageNum1,pool->fh,pool->strtFrame->bph->data);
					if(status == RC_OK)
					{
						page->data = pool->strtFrame->bph->data;
						page->pageNum = pageNum1;
						pool->strtFrame->bph->pageNum = pageNum1;
						pool->strtFrame->pageFrameNum = empty;
						pool->strtFrame->is_dirty = 0;
						pool->strtFrame->pageFrameCount = 1;
						pool->strtFrame->freqCount = 1;
						pool->strtFrame->fixcounts = 1;
						pool->strtFrame->pageNum = pageNum1;
						pool->strtFrame->nxtFrame = NULL;
						pool->currFrame = pool->strtFrame;
					}
					else
					{
						printf("Pin Page failed because of: %d \n", status);
						pthread_mutex_unlock(&mutex_pinPage); 
						return RC_BUFFER_POOL_ERROR_IN_PINPAGE;
					}
				}
				else
				{
					pool->currFrame->nxtFrame = (PageFrame *)malloc(sizeof(PageFrame));
					pool->currFrame = pool->currFrame->nxtFrame;
					pool->currFrame->bph = MAKE_PAGE_HANDLE();
					pool->currFrame->bph->data = (char *) malloc(PAGE_SIZE);
					pool->currFrame->nxtFrame = (PageFrame *)malloc(sizeof(PageFrame));
					int a = readBlock(pageNum1,pool->fh,pool->currFrame->bph->data);
					if(a == RC_OK)
					{
						page->data =pool->currFrame->bph->data;
						page->pageNum = pageNum1;
						pool->currFrame->bph->pageNum = pageNum1;
						pool->currFrame->pageFrameNum = empty;
						pool->currFrame->is_dirty = 0;
						pool->currFrame->pageFrameCount = 1;
						pool->currFrame->freqCount = 1;
						pool->currFrame->fixcounts = 1;
						pool->currFrame->pageNum = pageNum1;
						pool->currFrame->nxtFrame = NULL;
						ChangeCount(bm->mgmtData);
						pool->currFrame->pageFrameCount = 1;
					}
					else
					{
						printf("Pin Page failed because of: %d \n", a);
						pthread_mutex_unlock(&mutex_pinPage); // Unlock mutex while returning
						return RC_BUFFER_POOL_ERROR_IN_PINPAGE;
					}
				}
			}
			else
			{
				int status;
				if(bm->strategy == RS_LFU)
				{
					status = LFU(bm, page, pageNum1);//implementing LFU strategy
				}
				else
				{
					status = replacementStrategy(bm, page, pageNum1);//Implementing the Specific Strategy 
				}
				if(status != RC_OK)
				{
					printf("Pin Page failed because of: %d \n", status);
					pthread_mutex_unlock(&mutex_pinPage);// Unlock mutex while returning
					return RC_BUFFER_POOL_ERROR_IN_PINPAGE;
				}
			}
		}
		else
		{
			pool->search->fixcounts += 1;
			page->data = pool->search->bph->data;
			page->pageNum = pageNum1;
			if(bm->strategy == RS_LRU)
			{
				ChangeCount(bm->mgmtData);
				pool->search->pageFrameCount = 1;
			}
			if(bm->strategy == RS_LFU)
			{			
				pool->search->freqCount += 1;
			}
			pthread_mutex_unlock(&mutex_pinPage);
			return RC_OK;
			
		}
		pool->numReadIO += 1;
		pthread_mutex_unlock(&mutex_pinPage);
		return RC_OK;
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;
	}
	
}

////////////////////////////////////////////////////////////////
//Return the contents of the page frame
////////////////////////////////////////////////////////////////
PageNumber *getFrameContents (BM_BufferPool *const bm)
{
	int p;
	PageNumber *page=(PageNumber *)malloc(sizeof(PageNumber)*bm->numPages);	
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;	
	if(pool != NULL)
	{
		pool->node=pool->strtFrame;
		if(pool->node !=NULL)
		{							
			for(p=0;p< bm->numPages;p++)
			{
				page[p]=-1;
			}
			p=0;
			while(pool->node != NULL)
			{
				page[p]=pool->node->pageNum;
				p++;
				pool->node=pool->node->nxtFrame;				
			}			
			
		}
		else
		{
			return NO_PAGE;
		}			
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;	
	}
	
	return page;
}
////////////////////////////////////////////////////////////////
//Get and return Fix Counts of BufferPool
////////////////////////////////////////////////////////////////
int *getFixCounts (BM_BufferPool *const bm)
{
	int f;
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;
	int *fcounts = (int *)malloc(sizeof(int)*(bm->numPages));		
	//check if the BufferPool is initialized
	if(pool != NULL)
	{
		pool->node=pool->strtFrame;
		if(pool->node !=NULL)
		{
									
			for(f=0;f< bm->numPages;f++)
			{
				fcounts[f]=0;
			}
			f=0;
			while(pool->node != NULL)
			{
				fcounts[f]=pool->node->fixcounts;
				f++;
				pool->node=pool->node->nxtFrame;				
			}			
		}
		else
		{
			return NO_PAGE;
		}			
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;	
	}
	return fcounts;
}

////////////////////////////////////////////////////////////////
//Get and return number of Read IO of BufferPool
////////////////////////////////////////////////////////////////
int getNumReadIO (BM_BufferPool *const bm)
{
	if(bm->mgmtData != NULL)
	{
		return ((BM_BufferPoolInfo *)bm->mgmtData)->numReadIO;
	}
	else
	{
		return 0;
	}
	
}

////////////////////////////////////////////////////////////////
//Get and return number of Write IO of BufferPool
////////////////////////////////////////////////////////////////
int getNumWriteIO (BM_BufferPool *const bm)
{
	if(bm->mgmtData != NULL)
	{
		return ((BM_BufferPoolInfo *)bm->mgmtData)->numWriteIO;
	}
	else
	{
		return 0;
	}
}

////////////////////////////////////////////////////////////////
//Check if the page is dirty or not and return accordingly
////////////////////////////////////////////////////////////////
bool *getDirtyFlags(BM_BufferPool *const bm)
{		
	int d;
	BM_BufferPoolInfo *pool=(BM_BufferPoolInfo *)bm->mgmtData;
	bool *dirty = (bool *)malloc(sizeof(bool)*bm->numPages);	
	
	//check if the BufferPool is initialized
	if(pool != NULL)
	{
		pool->node=pool->strtFrame;
		if(pool->node !=NULL)
		{
										
			for(d=0;d< bm->numPages;d++)
			{
				dirty[d]=FALSE;
			}
			d=0;
			while(pool->node != NULL)
			{
				dirty[d]=pool->node->is_dirty;
				d++;
				pool->node=pool->node->nxtFrame;				
			}			
			
		}
		else
		{
			return NO_PAGE;
		}			
	}
	else
	{
		return RC_BUFFER_POOL_NOT_INIT;	
	}
	return dirty;
}





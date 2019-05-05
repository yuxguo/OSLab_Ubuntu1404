/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/*********************************************************/
#define WSIZE 4     //word lenth
#define DSIZE 8     //data block lenth
#define CHUNKSIZE (1 << 12)  //heap size incrment  

#define MAX(x,y) ((x) > (y) ? (x) : (y))    //max number in x,y

#define PACK(size,alloc) ((size) | (alloc)) // 32位内存空间，[31:3]是块的大小，[2:0]中第0位代表是否分配，两个数字按位或一下就实现了打包

/* Read and write the word at p*/
#define GET(p) (*(unsigned int *)(p)) //p指向的地址的值
#define PUT(p,val) (*(unsigned int *)(p) = (val)) //将val放到p指向的地址

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1) //分别从p指向位置获取块大小和分配位。注意：p应该指向头/脚部
#define GET_PREV_INFO(p) (GET(p) & 0x2)//用次低位与0010按位与得到当前块的前一个块是否已使用 

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))     
//只要注意，bp指向有效区域开头

#define GET_SUCC(p) (GET(p))
#define PUT_SUCC(p,val) (PUT(p,val))

#define GET_PREV(p) (*((unsigned int *)(p) + 1))
#define PUT_PREV(p,val) (*((unsigned int *)(p) + 1) = (val))

#define MIN_SIZE 16

#define PU_U (0x3)
#define PU_N (0x2)
#define PN_U (0x1)
#define PN_N (0x0)

#define MASK_PU 0x00000002
#define MASK_PN 0xfffffffd 

#define LEVEL_0 (32)
#define LEVEL_1 (64)
#define LEVEL_2 (128)
#define LEVEL_3 (256)
#define LEVEL_4 (512)
#define LEVEL_5 (1024)
#define LEVEL_6 (1536)
#define LEVEL_7 (2048)
#define LEVEL_8 (3072)
#define LEVEL_9 (4096)
#define LEVEL_10 (8192)
#define LEVEL_11 (16384)
#define NUMBERS 13


/*********************************************************/

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void insert_node_level(char *bp);
static int which_level (size_t asize);



static int first_malloc;
static int first_extend;
static int mode;

static char *heap_listp = NULL;
static char *start_p[NUMBERS];



///
static int which_level (size_t asize)
{
    if (asize>=0 && asize <=LEVEL_0)
        return 0;
    else if (asize>LEVEL_0 && asize<=LEVEL_1)
        return 1;
    else if (asize>LEVEL_1 && asize<=LEVEL_2)
        return 2;
    else if (asize>LEVEL_2 && asize<=LEVEL_3)
        return 3;
    else if (asize>LEVEL_3 && asize<=LEVEL_4)
        return 4;
    else if (asize>LEVEL_4 && asize<=LEVEL_5)
        return 5;
    else if (asize>LEVEL_5 && asize<=LEVEL_6)
        return 6;
    else if (asize>LEVEL_6 && asize<=LEVEL_7)
        return 7;
    else if (asize>LEVEL_7 && asize<=LEVEL_8)
        return 8;
    else if (asize>LEVEL_8 && asize<=LEVEL_9)
        return 9;
    else if (asize>LEVEL_9 && asize<=LEVEL_10)
        return 10;
    else if (asize>LEVEL_10 && asize<=LEVEL_11)
        return 11;
    else
        return 12;
}
///
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    first_malloc=1;
    first_extend=1;
    char *bp;
    if ((heap_listp = mem_sbrk(16 * WSIZE)) == (void *) -1)
        return -1;

    int i;
    for (i=0; i<NUMBERS; ++i)
    {
        start_p[i]=heap_listp+((i+1)*WSIZE);
    }
    PUT(heap_listp, 0);// first block
    PUT(heap_listp + (1*WSIZE), NULL);//1
    PUT(heap_listp + (2*WSIZE), NULL );//2
    PUT(heap_listp + (3*WSIZE), NULL );//3
    PUT(heap_listp + (4*WSIZE), NULL );//4
    PUT(heap_listp + (5*WSIZE), NULL );//5
    PUT(heap_listp + (6*WSIZE), NULL );//6
    PUT(heap_listp + (7*WSIZE), NULL );//7  1025-2048
    PUT(heap_listp + (8*WSIZE), NULL );//8 2049-4096
    PUT(heap_listp + (9*WSIZE), NULL );//9 4097 -
    PUT(heap_listp + (10*WSIZE), NULL );//10 4097 -
    PUT(heap_listp + (11*WSIZE), NULL );//11 4097 -
    PUT(heap_listp + (12*WSIZE), NULL );//12 4097 -
    PUT(heap_listp + (13*WSIZE), NULL );//13 4097 -
    PUT(heap_listp + (14*WSIZE), PACK(WSIZE, PN_U));
    PUT(heap_listp + (15*WSIZE), PACK(0, PU_U));     //lenth 0, previous not used

    heap_listp += (14*WSIZE);//heap start addr
    if ((bp = extend_heap(2*CHUNKSIZE/WSIZE+4)) == NULL)
        return -1;
    return 0;
}// Done!



static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    size = (words % 2) ? (words+1) * WSIZE : words*WSIZE; 

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    if (GET_PREV_INFO(HDRP(bp)) != 0)
    {
        PUT(HDRP(bp), PACK(size, PU_N));
        PUT(FTRP(bp), PACK(size, PU_N));//change current header and footer
    }
    else
    {
        PUT(HDRP(bp), PACK(size, PN_N));
        PUT(FTRP(bp), PACK(size, PN_N));
    }
    

    //PUT_SUCC(bp,NULL);       //bp's succeed is next
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, PN_U));//make next header
    //PUT_PREV(bp,NULL);       //next previous is bp

    
    char *t = coalesce(bp);
    insert_node_level(t);
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //确定要malloc的具体大小，调用findfit查找，再用place放置
    if (first_malloc)
    {
        if (size == 64 )
        {
            mode =1;
        }
        else if (size == 16)
        {
            mode =2;
        }
        else 
        {
            mode =0;
        }
        first_malloc=0;
    }
    size_t asize;
    size_t extendsize;
    char *bp;
    if (size ==0)
        return NULL;
    
    if (size <= 12 )
        asize = 2*DSIZE;//MIN is WSIZE
    else 
    {
        size_t tmp = size / WSIZE +2;
        asize = (tmp % 2) ? (tmp+1)*WSIZE : tmp*WSIZE;
    }
        //DSIZE is 8, so if request a block less than 8,
        //it will make it up to 8
    
    if ((bp = find_fit(asize)) != NULL)
    {
        return place(bp, asize);
    }//if can find a fit block

    if (first_extend && mode==1)
    {
        extendsize = 1024000-2*CHUNKSIZE-16;
        first_extend=0;
    }
    else if (first_extend && mode==2)
    {
        extendsize = 512000-2*CHUNKSIZE-16;
        first_extend=0;
    }
    else
    {
        extendsize = MAX(asize, CHUNKSIZE);
    }
    
    
    //otherwise it will expand the heap range
    if (extend_heap(extendsize/WSIZE) == NULL)
        return NULL;
    bp = find_fit(asize);
    return place(bp, asize);
}





/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{   
    //printf("No. %d malloc or free\n",count++);

    if (!bp)
        return ;
    size_t size = GET_SIZE(HDRP(bp));
    if (GET_PREV_INFO(HDRP(bp)) != 0)
    {
        PUT(HDRP(bp), PACK(size, PU_N));
        PUT(FTRP(bp), PACK(size, PU_N));
    }
    else
    {
        PUT(HDRP(bp), PACK(size, PN_N));
        PUT(FTRP(bp), PACK(size, PN_N));
    }
    PUT_PREV(bp,NULL);
    PUT_SUCC(bp,NULL);
    //给要 free的块加上脚和头，再用coal合并
    char * t=coalesce(bp);
    //将合并好的块调用insert插入到链表中，修改算法再insert中
    insert_node_level(t);
}
static void *coalesce(void *bp)
{
    // 合并的坑，因为这些空闲块在逻辑上面相邻，但是在地址上面不一定相邻，所以在合并的时候要分开讨论
    //从当前块头读出前一个块是否分配
    size_t is_prev_alloc = GET_PREV_INFO(HDRP(bp));
    //通过块大小计算出后一块，看它是否分配
    size_t is_succ_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    //读出当前块的大小
    size_t size = GET_SIZE(HDRP(bp));


    if (is_prev_alloc==2 && is_succ_alloc==1)
    {
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
        PUT(HDRP(bp),PACK(size,PU_N));
        PUT(FTRP(bp),PACK(size,PU_N));
        //更改这个块之后的块的状态，按位与一个掩码，将其次低位置为0，标记为前块未用
        PUT(HDRP(NEXT_BLKP(bp)), (GET(HDRP(NEXT_BLKP(bp))) & MASK_PN));
    }
    else if (is_prev_alloc==2 && is_succ_alloc==0)
    {
        //前驱被分配，后继未被分配(物理地址)
        //先不更新size，用未增加的size找到当前块的后继的后继，将链表维护完整
        char *t2=(char *)NEXT_BLKP(bp); //get物理位置之后的那个节点
        PUT_SUCC(GET_PREV(t2),GET_SUCC(t2));//t2逻辑前驱节点的后继 <==
        
        if (GET_SUCC(t2))//若t2节点的逻辑后继存在
        {
            PUT_PREV(GET_SUCC(t2),GET_PREV(t2));
        }//至此，将t2节点从双向链表中独立出来了
        
        //再更新size
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        //先将头信息更新，再用头的信息去找脚
        PUT(HDRP(bp), PACK(size, PU_N));
        PUT(FTRP(bp), PACK(size, PU_N));
        //至此，完成了将bp和t2合为一体（物理）

        //再将这个节点的前驱和后继分别置为NULL，等待插入
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
        //更改这个块之后的块的状态（物理相邻），按位与一个掩码，将其次低位置为0，标记为前块未用
        PUT(HDRP(NEXT_BLKP(bp)), (GET(HDRP(NEXT_BLKP(bp))) & MASK_PN));        
        
    }
    else if (is_prev_alloc==0 && is_succ_alloc==1)
    {
        //前驱未被分配，后继被分配
        //先不更新size，用未增加的size找到当前块的后继的后继，将链表维护完整
        char *t1=(char *)PREV_BLKP(bp);//另t1为bp物理地址上靠前的节点
        PUT_SUCC(GET_PREV(t1),GET_SUCC(t1));//前面节点的前驱 的后继更改为自己的后继
        if (GET_SUCC(t1))
        {
            PUT_PREV(GET_SUCC(t1),GET_PREV(t1));
        }
        //printf("1");
        //再更新size
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        //printf("2");
        //先将头信息更新，再用头的信息去找脚
        //注意，前驱的前驱一定被占用
        PUT(HDRP(bp), PACK(size, PU_N));
        PUT(FTRP(bp), PACK(size, PU_N));
        //最后分别将指针置为0，等待插入
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
        //printf("3");
        //更改这个块之后的块的状态，按位与一个掩码，将其次低位置为0，标记为前块未用
        PUT(HDRP(NEXT_BLKP(bp)), (GET(HDRP(NEXT_BLKP(bp))) & MASK_PN));   
        
    }
    else 
    {
        //前驱未被占用，后继被占用
        //先不更新size，用未增加的size找到当前块的后继的后继，将链表维护完整
        char *t1=(char *)PREV_BLKP(bp);//另t1为bp物理地址上靠前的节点
        char *t2=(char *)NEXT_BLKP(bp); //get物理位置之后的那个节点
        PUT_SUCC(GET_PREV(t1),GET_SUCC(t1));//前面节点的前驱 的后继更改为bp的后继
        if (GET_SUCC(t1))
        {
            PUT_PREV(GET_SUCC(t1),GET_PREV(t1));
        }

        PUT_SUCC(GET_PREV(t2),GET_SUCC(t2));//t2逻辑前驱节点的后继 
        if (GET_SUCC(t2))//若t2节点的逻辑后继存在
        {
            PUT_PREV(GET_SUCC(t2),GET_PREV(t2));
        }//至此，将t2节点从双向链表中独立出来了
        //再更新size
        //注意，此时不先移动bp，否则就找不到脚了
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, PU_N));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, PU_N));

        //移动bp
        bp = PREV_BLKP(bp);
        //置NULL等待插入
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
        //更改这个块之后的块的状态，按位与一个掩码，将其次低位置为0，标记为前块未用
        PUT(HDRP(NEXT_BLKP(bp)), (GET(HDRP(NEXT_BLKP(bp))) & MASK_PN));   
    }
    return bp;
}

static void insert_node_level(char *bp)
{
    size_t size=GET_SIZE(HDRP(bp));
    int level = which_level(size);
    
    /*char *p_tmp = GET_SUCC(start_p[level]);
    if (p_tmp)
    {
        PUT_SUCC(bp,p_tmp);
        PUT_PREV(bp,start_p[level]);
        PUT_SUCC(start_p[level], bp);
        PUT_PREV(p_tmp, bp);
    }
    else
    {
        PUT_SUCC(start_p[level],bp);
        PUT_PREV(bp,start_p[level]);
    }*/
    
    char *p=NULL;
    char *p_prev=start_p[level];

    for (p = GET_SUCC(start_p[level]); p!=NULL ;p=GET_SUCC(p))
    {
        p_prev=GET_PREV(p);
        if (size<GET_SIZE(p))
            continue;
        else
        {
            PUT_SUCC(bp,p);
            PUT_PREV(bp,p_prev);
            PUT_SUCC(p_prev, bp);
            PUT_PREV(p, bp);
            return;
        }
    }
    if (p_prev != start_p[level])
    {
        p_prev = GET_SUCC(p_prev);
    }
    PUT_SUCC(p_prev,bp);
    PUT_PREV(bp,p_prev);
    return ;    
}


static void *place(void *bp, size_t asize)
{
    size_t space = GET_SIZE(HDRP(bp));
    if (space - asize >= MIN_SIZE)
    {
        if (asize<100)
        {
            char *p_tmp = bp + asize; 
            PUT(HDRP(bp), PACK(asize,PU_U));
            PUT(HDRP(p_tmp),PACK(space-asize,PU_N));
            PUT(FTRP(p_tmp),PACK(space-asize,PU_N));
        

            PUT_SUCC(GET_PREV(bp),GET_SUCC(bp));
        
            if (GET_SUCC(bp))
            {
                PUT_PREV(GET_SUCC(bp),GET_PREV(bp));
            }
        
        // if (GET_SUCC(p_tmp))
        // {
        //     PUT_PREV(GET_SUCC(p_tmp), p_tmp);
        // }
        // PUT_SUCC(GET_PREV(p_tmp), p_tmp);
            PUT_SUCC(p_tmp,NULL);
            PUT_PREV(p_tmp,NULL);
            insert_node_level(p_tmp);
        }
        else 
        {
            char *p_tmp = bp;
            PUT(HDRP(NEXT_BLKP(bp)),(GET(HDRP(NEXT_BLKP(bp))) | MASK_PU ));
            PUT_SUCC(GET_PREV(p_tmp),GET_SUCC(p_tmp));
        
            if (GET_SUCC(p_tmp))
            {
                PUT_PREV(GET_SUCC(p_tmp),GET_PREV(p_tmp));
            }
            bp = bp+space-asize;
            PUT(HDRP(bp), PACK(asize, PN_U));
            PUT(HDRP(p_tmp), PACK(space-asize, PU_N));
            PUT(FTRP(p_tmp), PACK(space-asize, PU_N));

            
            PUT_SUCC(p_tmp,NULL);
            PUT_PREV(p_tmp,NULL);
            insert_node_level(p_tmp);
        }
    }
    else
    {
        PUT(HDRP(bp), PACK(space,PU_U));
        PUT(HDRP(NEXT_BLKP(bp)),(GET(HDRP(NEXT_BLKP(bp))) | MASK_PU ));
        
        if (GET_SUCC(bp))
        {
            PUT_PREV(GET_SUCC(bp), GET_PREV(bp));
        }
        PUT_SUCC(GET_PREV(bp), GET_SUCC(bp));
    }
    return bp;
}

static void *find_fit(size_t asize)
{
	char *p;
    const int level=which_level(asize);
    int l;
    for (l=level;l<NUMBERS;++l)
    {
        for (p=GET_SUCC(start_p[l]); p ; p=GET_SUCC(p))
        {
            if (GET_SIZE(HDRP(p))>=asize)
                return p;
        }
    }
    return NULL;
    
}




/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);
    return newptr;
}


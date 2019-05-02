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

#define GET_PREV(p) (GET(p))
#define PUT_PREV(p,val) (PUT(p,val))

#define GET_SUCC(p) (GET( ((unsigned int *)(p) + 1)) )
#define PUT_SUCC(p,val) (PUT( ((unsigned int *)(p) + 1), val ))

#define MIN_SIZE 16

#define PU_U (0x3)
#define PU_N (0x2)
#define PN_U (0x1)
#define PN_N (0x0)
/*********************************************************/

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void insert_node_LIFO(char *bp);
static char *heap_listp = NULL;
static char *start_p = NULL;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    if ((heap_listp = mem_sbrk(8 * WSIZE)) == (void *) -1)
        return -1;

    start_p = heap_listp + WSIZE;
    PUT(heap_listp, 0);// first block
    PUT(heap_listp + (1*WSIZE), (unsigned int)NULL);
    PUT(heap_listp + (2*WSIZE), (unsigned int)(heap_listp + (6*WSIZE)) );
    PUT(heap_listp + (3*WSIZE), PACK(DSIZE, PN_U));
    PUT(heap_listp + (4*WSIZE), PACK(DSIZE, PN_U));
    PUT(heap_listp + (5*WSIZE), PACK(0, PN_U));     //lenth 0, previous not used
    PUT(heap_listp + (6*WSIZE), (unsigned int)start_p);           //previous start_p, also is bp 
    PUT(heap_listp + (7*WSIZE), (unsigned int)NULL);              //succeed is NULL

    heap_listp += (4*WSIZE);//heap start addr

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
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
    
    bp -= (2*WSIZE);//bp回到最后一个块的起始位置，回到有效载荷

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
    

    PUT_SUCC(bp,HDRP(NEXT_BLKP(bp)));       //bp's succeed is next
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, PN_U));//make next header
    PUT_PREV(HDRP(NEXT_BLKP(bp)),bp);       //next previous is bp
    PUT_SUCC(HDRP(NEXT_BLKP(bp)),NULL);        //next succeed is NULL

    return coalesce(bp);
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //确定要malloc的具体大小，调用findfit查找，再用place放置
    size_t asize;
    size_t extendsize;
    char *bp;
    if (size ==0)
        return NULL;
    
    if (size <= DSIZE)
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
        place(bp, asize);
        return bp;
    }//if can find a fit block

    extendsize = MAX(asize, CHUNKSIZE);
    //otherwise it will expand the heap range
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}



/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    if (GET_PREV_INFO(HDRP(bp)) != 0)
    {
        PUT(HDRP(bp), PACK(size, PU_N));
        PUT(FTRP(bp), PACK(size, PU_N));
    }
    else
    {
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    //给要 free的块加上脚和头，再用coal合并
    coalesce(bp);
    //将合并好的块调用insert插入到链表中，修改算法再insert中
    insert_node_LIFO(bp);
}
static void *coalesce(void *bp)
{
    //从当前块头读出前一个块是否分配
    size_t is_prev_alloc = GET_PREV_INFO(HDRP(bp));
    //通过块大小计算出后一块，看它是否分配
    size_t is_succ_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    //读出当前块的大小
    size_t size = GET_SIZE(HDRP(bp));

    if (is_prev_alloc && is_succ_alloc)
    {
        if (size < MIN_SIZE)
        {
            //如果释放的块的大小小于MIN_SIZE，
            //则需要将其并入这个块的前驱块的后面，作为填充块
            //没想好要怎么做
        }
        else
        {
            //若释放块的大小，大于空闲块所需要的最低大小，
            //则直接初始化为一个节点，等待插入链表
            PUT_PREV(bp,NULL);
            PUT_SUCC(bp,NULL);
            PUT(HDRP(bp),PACK(size,PU_N));
            PUT(FTRP(bp),PACK(size,PU_N));
        }
    }
    else if (is_prev_alloc && !is_succ_alloc)
    {
        //前驱被分配，后继未被分配
        //先更新size
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        //先将头信息更新，再用头的信息去找脚
        PUT(HDRP(bp), PACK(size, PU_N));
        PUT(FTRP(bp), PACK(size, PU_N));
        //再将这个节点的前驱和后继分别置为NULL，等待插入
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
        
    }
    else if (!is_prev_alloc && is_succ_alloc)
    {
        //前驱未被分配，后继被分配
        //先更新size，同时切换bp到前驱的bp
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);

        //先将头信息更新，再用头的信息去找脚
        //注意，前驱的前驱一定被占用
        PUT(HDRP(bp), PACK(size, PU_N));
        PUT(FTRP(bp), PACK(size, PU_N));
        //最后分别将指针置为0，等待插入
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
        
    }
    else 
    {
        //前驱未被占用，后继被占用
        //先更新大小
        //注意，此时不先移动bp，否则就找不到脚了
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, PU_N));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, PU_N));

        //移动bp
        bp = PREV_BLKP(bp);
        //置NULL等待插入
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
    }
    return bp;
}

static void insert_node_LIFO(char *bp)
{
    char *p_tmp = GET_SUCC(start_p);
    if (bp)
    {
        PUT_SUCC(bp,p_tmp);
        PUT_PREV(bp,start_p);
        PUT_SUCC(GET_SUCC(start_p), bp);
        PUT_PREV(GET_PREV(p_tmp), bp);
    }
    
}


static void place(void *bp, size_t asize)
{
    size_t space = GET_SIZE(HDRP(bp));
    if (space - asize >= MIN_SIZE)
    {
        char *p_tmp = bp + asize; 
        PUT(HDRP(bp), PACK(asize,PU_U));
        PUT(HDRP(p_tmp),PACK(space-asize,PU_N));
        PUT_SUCC(p_tmp,GET_SUCC(bp));
        PUT_PREV(p_tmp,GET_PREV(bp));
        PUT(FTRP(p_tmp),PACK(space-asize,PU_N));
    }
    else
    {
        PUT(HDRP(bp), PACK(space,PU_U));
        PUT_SUCC(GET_PREV(bp), GET_SUCC(bp));
        PUT_PREV(GET_SUCC(bp), GET_PREV(bp));
    }
}

static void *find_fit(size_t asize)
{
	char *p;
    for (p =(char *)GET_SUCC(start_p); p; p=(char *)GET_SUCC(p))
    {
        if (GET_SIZE(HDRP(p)) >= asize)
        {
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
    return NULL;
}


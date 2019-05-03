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

#define GET_SUCC(p) (*((unsigned int *)(p) + 1))
#define PUT_SUCC(p,val) (*((unsigned int *)(p) + 1) = (val))

#define MIN_SIZE 16

#define PU_U (0x3)
#define PU_N (0x2)
#define PN_U (0x1)
#define PN_N (0x0)

#define MASK_PU 0x00000002
#define MASK_PN 0xfffffffd 
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
    char *bp;
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *) -1)
        return -1;

    start_p = heap_listp + WSIZE;
    PUT(heap_listp, 0);// first block
    PUT(heap_listp + (1*WSIZE), NULL);
    PUT(heap_listp + (2*WSIZE), NULL );
    PUT(heap_listp + (3*WSIZE), PACK(DSIZE, PN_U));
    PUT(heap_listp + (4*WSIZE), PACK(DSIZE, PN_U));
    PUT(heap_listp + (5*WSIZE), PACK(0, PU_U));     //lenth 0, previous not used

    heap_listp += (4*WSIZE);//heap start addr
    printf("\nin mm_init\n");
    if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;

    printf("\n %d \n", (unsigned int)mem_sbrk(0)-(unsigned int)heap_listp-2*WSIZE );
    printf("\n %d \n",(unsigned int)GET_SIZE(HDRP(GET_SUCC(start_p))) );
    printf("\nout mm_init\n");
    return 0;
}// Done!

static void *extend_heap(size_t words)
{
    printf("\nin extend_heap\n");
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
    

    PUT_SUCC(bp,NULL);       //bp's succeed is next
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, PN_U));//make next header
    PUT_PREV(bp,NULL);       //next previous is bp

    printf("\nout extend_heap\n");
    
    char *t = coalesce(bp);
    insert_node_LIFO(t);
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //确定要malloc的具体大小，调用findfit查找，再用place放置
    printf("\nin mm_malloc\n");
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
        printf("\n %d \n", asize);
        place(bp, asize);
        return bp;
    }//if can find a fit block

    extendsize = MAX(asize, CHUNKSIZE);
    char *p;
    for (p =(char *)start_p; p; p=(char *)GET_SUCC(p))
    {
        printf("\n list=%x \n",p);
    }
    //otherwise it will expand the heap range
    if (extend_heap(extendsize/WSIZE) == NULL)
        return NULL;
    bp = find_fit(asize);
    place(bp, asize);
    printf("\nout mm_malloc\n");
    return bp;
}



/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    printf("\n in mm_free \n");
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
    //给要 free的块加上脚和头，再用coal合并
    coalesce(bp);
    //将合并好的块调用insert插入到链表中，修改算法再insert中
    insert_node_LIFO(bp);
    printf("\n out mm_free \n");
}
static void *coalesce(void *bp)
{
    // 合并的坑，因为这些空闲块在逻辑上面相邻，但是在地址上面不一定相邻，所以在合并的时候要分开讨论
    printf("\nin coalesce\n");
    //从当前块头读出前一个块是否分配
    size_t is_prev_alloc = GET_PREV_INFO(HDRP(bp));
    //通过块大小计算出后一块，看它是否分配
    size_t is_succ_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    //读出当前块的大小
    size_t size = GET_SIZE(HDRP(bp));
    printf ("\n %d, %d \n", is_prev_alloc, is_succ_alloc);
    printf("\n %x, %x \n", bp,mem_sbrk(0));

    if (is_prev_alloc && is_succ_alloc)
    {
        printf("1");
        //若释放块的大小，大于空闲块所需要的最低大小，
        //则直接初始化为一个节点，等待插入链表
        //同时需要特别注意，将这个节点拿出来的同时还要将它的前驱节点和后继节点相连
        /*PUT_SUCC(GET_PREV(bp), GET_SUCC(bp));//前驱节点的后继等于当前节点的后继
        if (GET_SUCC(bp))
        {
            PUT_PREV(GET_SUCC(bp),GET_PREV(bp));
        }*/
        PUT_PREV(bp,NULL);
        PUT_SUCC(bp,NULL);
        PUT(HDRP(bp),PACK(size,PU_N));
        PUT(FTRP(bp),PACK(size,PU_N));
        //更改这个块之后的块的状态，按位与一个掩码，将其次低位置为0，标记为前块未用
        PUT(HDRP(NEXT_BLKP(bp)), (GET(HDRP(NEXT_BLKP(bp))) & MASK_PN));
    }
    else if (is_prev_alloc && !is_succ_alloc)
    {
        printf("2");
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
    else if (!is_prev_alloc && is_succ_alloc)
    {
        //前驱未被分配，后继被分配
        //先不更新size，用未增加的size找到当前块的后继的后继，将链表维护完整
        printf("3");
        char *t1=(char *)PREV_BLKP(bp);//另t1为bp物理地址上靠前的节点
        printf(" t1=%x ", t1);
        char *p;
        for (p =(char *)start_p; p; p=(char *)GET_SUCC(p))
        {
            printf("\n list=%x \n",p);
        }
        PUT_SUCC(GET_PREV(t1),GET_SUCC(t1));//前面节点的前驱 的后继更改为bp的后继
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
        printf("4");
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
    printf("\nout coalesce\n");
    return bp;
}

static void insert_node_LIFO(char *bp)
{
    printf("\nin insert_node_LIFO\n");
    
    char *p_tmp = GET_SUCC(start_p);
    printf("\n %x, %x \n", bp, p_tmp);
    if (p_tmp)
    {
        PUT_SUCC(bp,p_tmp);
        PUT_PREV(bp,start_p);
        PUT_SUCC(start_p, bp);
        PUT_PREV(p_tmp, bp);
    }
    else
    {
        PUT_SUCC(start_p,bp);
        PUT_PREV(bp,start_p);
    }
    
    printf("\nout insert_node_LIFO\n");
}


static void place(void *bp, size_t asize)
{
    printf("\nin place\n");
    size_t space = GET_SIZE(HDRP(bp));
    printf("\n %d, %d \n",(unsigned int)GET_SIZE(HDRP(GET_SUCC(start_p))), space );
    if (space - asize >= MIN_SIZE)
    {
        printf ("\n 11 \n");
        char *p_tmp = bp + asize; 
        PUT(HDRP(bp), PACK(asize,PU_U));
        PUT(HDRP(p_tmp),PACK(space-asize,PU_N));
        PUT(FTRP(p_tmp),PACK(space-asize,PU_N));
        PUT_SUCC(p_tmp,GET_SUCC(bp));
        PUT_PREV(p_tmp,GET_PREV(bp));

        if (GET_SUCC(p_tmp))
        {
            PUT_PREV(GET_SUCC(p_tmp), p_tmp);
        }
        PUT_SUCC(GET_PREV(p_tmp), p_tmp);
        
    }
    else
    {
        printf ("\n 22 \n");        
        PUT(HDRP(bp), PACK(space,PU_U));
        PUT(HDRP(NEXT_BLKP(bp)),(GET(HDRP(NEXT_BLKP(bp))) | MASK_PU ));
        
        printf("\n %x, %x, %x \n", bp, GET_PREV(bp), GET_SUCC(bp));
        if (GET_SUCC(bp))
        {
            PUT_PREV(GET_SUCC(bp), GET_PREV(bp));
        }
        printf("\n 33 \n");
        PUT_SUCC(GET_PREV(bp), GET_SUCC(bp));
    }
    printf("\nout_place\n");
}

static void *find_fit(size_t asize)
{
    printf("\nin find_fit\n");
	char *p;
    for (p =(char *)GET_SUCC(start_p); p; p=(char *)GET_SUCC(p))
    {
        printf("\n %x \n",p);
        if (GET_SIZE(HDRP(p)) >= asize)
        {
            printf("\nout find_fit\n");
            return p;
        }
    }
    printf("\nout find_fit\n");
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


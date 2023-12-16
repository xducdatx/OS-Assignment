//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
static pthread_mutex_t mp_lock;

int MEMPHY_mv_csr(struct memphy_struct *mp, int offset)
{
   int numstep = 0;
   pthread_mutex_lock(&mp_lock);
   mp->cursor = 0;
   while(numstep < offset && numstep < mp->maxsz){
     /* Traverse sequentially */
     mp->cursor = (mp->cursor + 1) % mp->maxsz;
     numstep++;
   }
   pthread_mutex_unlock(&mp_lock);
   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value)
{
   pthread_mutex_lock(&mp_lock);
   if (mp == NULL)
   {
     pthread_mutex_unlock(&mp_lock);
     return -1;
   }

   if (!mp->rdmflg)
   {  
     pthread_mutex_unlock(&mp_lock);
     return -1; /* Not compatible mode for sequential read */
   }
   MEMPHY_mv_csr(mp, addr);
   *value = (BYTE) mp->storage[addr];
   pthread_mutex_unlock(&mp_lock);
   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
{
   if (mp == NULL)
     return -1;

   if (mp->rdmflg)
      *value = mp->storage[addr];
   else /* Sequential access device */
      return MEMPHY_seq_read(mp, addr, value);

   return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct * mp, int addr, BYTE value)
{
   pthread_mutex_lock(&mp_lock);
   if (mp == NULL)
   {
     pthread_mutex_unlock(&mp_lock);
     return -1;
   }
   if (!mp->rdmflg)
   {
     pthread_mutex_unlock(&mp_lock);
     return -1; /* Not compatible mode for sequential read */
   } 
   MEMPHY_mv_csr(mp, addr);
   mp->storage[addr] = value;
   pthread_mutex_unlock(&mp_lock);
   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct * mp, int addr, BYTE data)
{
   if (mp == NULL)
     return -1;

   if (mp->rdmflg)
      mp->storage[addr] = data;
   else /* Sequential access device */
      return MEMPHY_seq_write(mp, addr, data);

   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
    /* This setting come with fixed constant PAGESZ */
    int numfp = mp->maxsz / pagesz;
    struct framephy_struct *newfst, *fst;
    int iter = 0;

    if (numfp <= 0)
      return -1;

    /* Init head of free framephy list */ 
    fst = malloc(sizeof(struct framephy_struct));
    fst->fpn = iter;
    mp->free_fp_list = fst;
    pthread_mutex_lock(&mp_lock);
    /* We have list with first element, fill in the rest num-1 element member*/
    for (iter = 1; iter < numfp ; iter++)
    {
       newfst =  malloc(sizeof(struct framephy_struct));
       newfst->fpn = iter;
       newfst->fp_next = NULL;
       fst->fp_next = newfst;
       fst = newfst;
    }
    pthread_mutex_unlock(&mp_lock);
    return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn)
{
   pthread_mutex_lock(&mp_lock);
   struct framephy_struct *fp = mp->free_fp_list;

   if (fp == NULL)
   {
     pthread_mutex_unlock(&mp_lock);
     return -1;
   }
   *retfpn = fp->fpn;
   mp->free_fp_list = fp->fp_next;

   /* MEMPHY is iteratively used up until its exhausted
    * No garbage collector acting then it not been released
    */
   free(fp);
   pthread_mutex_unlock(&mp_lock);
   return 0;
}

int MEMPHY_dump(struct memphy_struct * mp)
{
    /*TODO dump memphy contnt mp->storage 
     *     for tracing the memory content
     */
   printf("------------------\n");
   for (int i = 0; i < mp->maxsz; i++) 
   {
      if (mp->storage[i] != 0)
      {
         printf("Addr 0x%x: %d\n", i ,mp->storage[i]);
      }
   }	
   printf("------------------\n");
   return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn)
{
   pthread_mutex_lock(&mp_lock);
   struct framephy_struct *fp = mp->free_fp_list;
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

   /* Create new node with value fpn */
   newnode->fpn = fpn;
   newnode->fp_next = fp;
   mp->free_fp_list = newnode;
   pthread_mutex_unlock(&mp_lock);
   return 0;
}


/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, int max_size, int randomflg)
{
   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   mp->maxsz = max_size;

   MEMPHY_format(mp,PAGING_PAGESZ);

   mp->rdmflg = (randomflg != 0)?1:0;

   if (!mp->rdmflg )   /* Not Ramdom acess device, then it serial device*/
      mp->cursor = 0;

   return 0;
}

//#endif

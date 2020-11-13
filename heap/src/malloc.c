/*
 * David Trimino
 * 1001659459
*/

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)      ((b) + 1)
#define BLOCK_HEADER(ptr)   ((struct _block *)(ptr) - 1)


static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit.  Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;         /* Size of the allocated _block of memory in bytes */
   struct _block *prev;  /* Pointer to the previous _block of allcated memory   */
   struct _block *next;  /* Pointer to the next _block of allcated memory   */
   bool   free;          /* Is this _block free?                     */
   char   padding[3];
};


struct _block *heapList = NULL; /* Free list to track the _blocks available */
struct _block *tracker  = NULL; /* Helps keep track of the current block when using next fit */
/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 */
struct _block *findFreeBlock(struct _block **last, size_t size) 
{
   struct _block *curr = heapList;

#if defined FIT && FIT == 0
   /* First fit */
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }
#endif

#if defined BEST && BEST == 0
   /* Best fit */
   struct _block* smallest = NULL;/* pointer to block gives the least space after allocation */ 
   int comparison = INT_MAX;      /* helps identify the smallest space after allocation */  
   while ( curr && !(curr->free && curr->size >= size) ){
      int current_size = (int) curr->size;
      if ( current_size < comparison ){
         smallest = curr;
         comparison = smallest->size;
      }
      curr = curr->next;
   }
   curr = smallest;
#endif

#if defined WORST && WORST == 0
   /* Worst fit */
   struct _block *largest = NULL; /* pointer to block gives the most space after allocation */
   int comparison = INT_MIN;      /* helps identify the largest space after allocation */
   while ( curr && !(curr->free && curr->size >= size) ){
      int current_size = (int) curr->size;
      if ( current_size > comparison ){
         largest = curr;
         comparison = largest->size;
      }
      curr = curr->next;
   }
   curr = largest;
#endif

#if defined NEXT && NEXT == 0
   /* Next fit */
   if ( tracker != NULL ) {
      curr = tracker;
   }
   while ( curr && !(curr->free && curr->size >= size) ){
      *last = curr;
      curr = curr->next;
   }
   tracker = curr;
#endif

   return curr;
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size) 
{
   /* Request more space from OS */
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   /* OS allocation failed */
   if (curr == (struct _block *)-1) 
   {
      return NULL;
   }

   /* Update heapList if not set */
   if (heapList == NULL) 
   {
      heapList = curr;
   }

   /* Attach new _block to prev _block */
   if (last) 
   {
      last->next = curr;
   }

   /* Update _block metadata */
   curr->size = size;
   curr->next = NULL;
   curr->free = false;
   return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size) 
{

   if( atexit_registered == 0 )
   {
      atexit_registered = 1;
      atexit( printStatistics );
   }

   /* Align to multiple of 4 */
   size = ALIGN4(size);

   /* Handle 0 size */
   if (size == 0) 
   {
      return NULL;
   }

   /* Look for free _block */
   struct _block *last = heapList;
   struct _block *next = findFreeBlock(&last, size);

   
   const int block_size = sizeof( struct _block );
  
   /* block is larger than what we need then split block */
   if ( next && next->size - size > block_size ){ 
      int old_size = next->size;
      struct _block *old_next = next;
      uint8_t *ptr = (uint8_t*) next;
      
      if ( next->next ){
         next->next = (struct _block*) (ptr + (int) size + block_size );
         next->next->free = true;
         next->next->size = (size_t) (old_size - (int) size - block_size );
         next->next->next = old_next;
      }

      num_splits++;
      num_reuses++;
   }

   /* Could not find free _block, so grow heap */
   if (next == NULL) 
   {
      next = growHeap(last, size);
   }

   /* Could not find free _block or grow heap, so just return NULL */
   if (next == NULL) 
   {
      return NULL;
   }
  
   /* Mark _block as in use */
   next->free = false;

   /* Increment coutners */
   num_mallocs++;
   num_requested+=size;

   /* Return data address associated with _block */
   return BLOCK_DATA(next);
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr) 
{
   if (ptr == NULL) 
   {
      return;
   }

   /* Make _block as free */
   struct _block *curr = BLOCK_HEADER(ptr);
   assert(curr->free == 0);
   curr->free = true;

   /* TODO: Coalesce free _blocks if needed */
   const int block_size = sizeof( struct _block );
   num_frees++;
}


void *realloc(void ptr*, size_t size){
   /* declare new ptr then malloc the new pointer */
   struct _block *new_ptr = malloc( size );

   if (ptr && new_ptr ){
      memcpy( new_ptr, ptr, size );
   }
   return new_ptr;
}

void *calloc( size_t num, size_t, size_num ){
   struct _block *ptr = malloc( num * size_num );
   if ( ptr ) {
      memset( ptr, '/0', num * size );
   }
   return ptr;
}

/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/

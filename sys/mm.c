
#include <defs.h>
#include <sys/mm.h>
#include <sys/k_stdio.h>

uint32_t page_num;

page_t *page_struct_begin;
uint32_t page_index_begin;

/* default page tables */
addr_t def_pgt_paddr_lv1;
addr_t def_pgt_paddr_lv2;
addr_t def_pgt_paddr_lv3;

/* start of usable pages */
addr_t page_begin;
addr_t page_begin_addr;


/* first kernel vma */
vma_t kvma_head;
uint64_t kvma_end;

/* =========================================================
 * Page Structure        
 * ====================================================== */


/* initialize page structures */
int
init_page
(
    uint32_t    occupied    /* occupied page struct num */
)
{
    uint32_t    i;
    page_t *    page_tmp;
    uint32_t    index = page_index_begin;

    for ( i=0; i<page_num; ++i, ++index )    {

        page_tmp = page_struct_begin + i;

        if ( i < occupied ) {
            page_tmp->flag      = PG_OCP | PG_SUP;
            page_tmp->count     = 1;
            page_tmp->va        = ((addr_t)&kernofs)+(index<<__PAGE_SIZE_SHIFT);
        }
        else                {
            page_tmp->flag      = PG_FRE | PG_USR;
            page_tmp->count     = 0;
            page_tmp->va        = 0x0;
        }             

        if ( i == page_num-1 )
            page_tmp->next      = 0;
        else
            page_tmp->next      = index+1;

        page_tmp->idx           = index;
        page_tmp->kmalloc_size  = 0x0;
        page_tmp->reserv64      = 0x0;

        //if ( i==1 || i==200 || i==page_num-1 ) {
        //    k_printf( 0, "init page: flag  : %x\n", page_tmp->flag );
        //    k_printf( 0, "init page: count : %x\n", page_tmp->count);
        //    k_printf( 0, "init page: idx   : %x\n", page_tmp->idx  );
        //    k_printf( 0, "init page: va    : %x\n", page_tmp->va   );
        //    k_printf( 0, "init page: next  : %x\n", page_tmp->next );
        //    k_printf( 0, "\n" );
        //}

    }

    return 0;
}/* init_page() */



page_t *
get_page_from_va
(
    void    *va
)
{
    pgt_t *pgt_tmp = get_pgt_entry_lv4( (addr_t)va );
    uint64_t page_idx = (uint64_t)pgt_tmp->paddr-(uint64_t)page_index_begin;
    return (page_struct_begin+page_idx);
}/* get_page_from_va() */


/* FIXME: should be deprecated in the future */
void *
get_va_from_page
(
    page_t  *page
)
{
    return (void *)(page->va);
}/* get_va_from_page() */


addr_t
get_pa_from_page
(
    page_t  *page
)
{
    return (page->idx)<<__PAGE_SIZE_SHIFT;
}/* get_pa_from_page() */


addr_t
get_pa_from_va
(
    void    *va
)
{
    return get_pa_from_page( get_page_from_va(va) );
}/* get_pa_from_page() */


/* find first N free pages */
uint32_t
find_free_pages
(
    uint32_t    num
)
{
    uint32_t i=0;
    uint32_t j=0;
    uint32_t free_start = 0;
    uint32_t free_cnt   = 0;
    page_t *page_tmp=NULL;
    for (i=0; i<(page_num-num+1); ++i) {

        if ( (i+num) > page_num )
            break;

        page_tmp = page_struct_begin + i;
        if ( !((page_tmp->flag) & PG_OCP) ) {
            free_cnt = 0;
            for (j=1; j<num; ++j) {
                page_tmp = page_struct_begin + (i+j);
                if ( !((page_tmp->flag) & PG_OCP) )
                    free_cnt++;
                else
                    break;
            }

            if ( (free_cnt+1)==num ) {
                free_start = i;
                break;
            }
            else
                i += j;
        } /* if find a free page */
    } /* loop all pages */

    return free_start;
}/* find_free_page */ 


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+   Get/Set Page Table Entry
*/

/* initialize a page table */
int
init_pgt
(
    addr_t      addr        /* page table virtual address */
)
{
    int i;
    volatile pgt_t *pgt_temp;
    for ( i=0; i<PGT_ENTRY_NUM; ++i ) {
        pgt_temp = (volatile void *)(addr+(i*8));
        pgt_temp->paddr     = 0x0   ;
        pgt_temp->present   = PGT_NP;
        pgt_temp->nx        = PGT_NX;
        pgt_temp->avl_1     = 0x0   ;
        pgt_temp->avl_2     = 0x0   ;
        pgt_temp->flag      = PGT_RO | PGT_SUP;
    }
    return 0;
}/* init_pgt */


/* set a page table entry */
int
set_pgt_entry
(
    addr_t      addr    , /* page table virtual address             */
    uint64_t    paddr   , /* page table entry index/physical address*/
    uint08_t    present , /* present bit                            */
    uint08_t    nx      , /* nx bit                                 */
    uint08_t    avl_1   , /* available to software                  */
    uint16_t    avl_2   , /* available to software                  */
    uint08_t    flag      /* flags                                  */
)
{
    volatile pgt_t *pgt_temp   = (volatile void *)(addr);

    pgt_temp->paddr     = paddr>>__PAGE_SIZE_SHIFT;
    pgt_temp->present   = present;
    pgt_temp->nx        = nx     ;
    pgt_temp->avl_1     = avl_1  ;
    pgt_temp->avl_2     = avl_2  ;
    pgt_temp->flag      = flag   ;

    return 0;
}/* set_pgt_entry */


/* set a lv4 page table entry */
int
set_pgt_entry_lv4
(
    addr_t      addr    , /* virtual address                        */
    uint64_t    paddr   , /* page table entry index/physical address*/
    uint08_t    present , /* present bit                            */
    uint08_t    nx      , /* nx bit                                 */
    uint08_t    avl_1   , /* available to software                  */
    uint16_t    avl_2   , /* available to software                  */
    uint08_t    flag      /* flags                                  */
)
{
    addr_t addr_tmp = 0xFFFF000000000000 | ((uint64_t)PGT_ENTRY_LV1_SELFREF<<39);
    addr_tmp = addr_tmp | ((addr>>9)&0x7FFFFFFFFF);
    return set_pgt_entry( addr_tmp, paddr, present, nx, avl_1, avl_2, flag );
}/* set_pgt_entry_lv4 */

/* get a lv4 page table entry */
pgt_t *
get_pgt_entry_lv4
(
    addr_t      addr      /* virtual address                        */
)
{
    addr_t addr_tmp = 0xFFFF000000000000 | ((uint64_t)PGT_ENTRY_LV1_SELFREF<<39);
    addr_tmp = addr_tmp | ((addr>>9)&0x7FFFFFFFFF);
    return (pgt_t *)addr_tmp;
}/* get_pgt_entry_lv4 */

/* set a lv1 page table entry */
int
set_pgt_entry_lv1
(
    addr_t      entry   , /* entry in lv1 page table                */
    uint64_t    paddr   , /* page table entry index/physical address*/
    uint08_t    present , /* present bit                            */
    uint08_t    nx      , /* nx bit                                 */
    uint08_t    avl_1   , /* available to software                  */
    uint16_t    avl_2   , /* available to software                  */
    uint08_t    flag      /* flags                                  */
)
{
    addr_t addr_tmp = 0xFFFF000000000000 
                      | ((uint64_t)PGT_ENTRY_LV1_SELFREF<<39) 
                      | ((uint64_t)PGT_ENTRY_LV1_SELFREF<<30) 
                      | ((uint64_t)PGT_ENTRY_LV1_SELFREF<<21) 
                      | ((uint64_t)PGT_ENTRY_LV1_SELFREF<<12);
    addr_tmp = addr_tmp | entry<<3;
    return set_pgt_entry( addr_tmp, paddr, present, nx, avl_1, avl_2, flag );
}/* set_pgt_entry_lv1 */

/*- Get/Set Page Table Entry
 *--------------------------------------------------------*/


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+   Get/Free Pages
*/

page_t *
alloc_free_pages
(
    uint16_t    flag        ,
    uint32_t    num 
)
{
    page_t *page_tmp    = NULL;
    page_t *page_start  = NULL;
    uint16_t pgt_flag   = PGT_RW;
    uint32_t page_struct_index = 0;
    int i;

    page_struct_index = find_free_pages( num );
    if (page_struct_index==0) {
        k_printf( 0, "get pages fail\n" );
        return NULL;
    }

    page_start = page_struct_begin + page_struct_index;
    kvma_end = (kvma_end&0xFFF) ? (kvma_end&0xFFFFFFFFFFFFF000)+0x1000
                                : kvma_end;
    page_tmp   = page_start;
    for ( i=0; i<num; ++i, kvma_end+=__PAGE_SIZE, page_tmp+=1 ) {
   
        if ( flag & PG_USR )
            pgt_flag |= PGT_USR;
        else
            pgt_flag |= PGT_SUP;
            
 
        set_pgt_entry_lv4( kvma_end, (page_tmp->idx)<<__PAGE_SIZE_SHIFT, PGT_P, PGT_NX,
                           0x0, 0x0, pgt_flag );
        
        page_tmp->flag  = flag | PG_OCP;
        page_tmp->count = 1;
        page_tmp->va    = kvma_end;
    }

    k_printf( 0, "alloc %d page(s). address: %16x, flag: %4x, kmalloc_size: %4d \n",
              num, page_start->va, page_start->flag, page_start->kmalloc_size );
    return page_start;
}/* alloc_free_pages() */


page_t *
alloc_pages
(
    uint16_t    flag        ,
    uint32_t    order 
)
{
    uint32_t num = ((uint32_t)1)<<order;
    return alloc_free_pages( flag, num );
}/* alloc_pages() */



page_t *
alloc_page
(
    uint16_t    flag        
)
{
    return alloc_pages( flag, 0 );
}/* alloc_page() */


void *
__get_free_pages
(
    uint16_t    flag        ,
    uint32_t    order 
)
{
    page_t *page_tmp = alloc_pages( flag, order );
    return get_va_from_page( page_tmp );
}/* __get_free_pages() */


void *
__get_free_page
(
    uint16_t    flag        
)
{
    return __get_free_pages( flag, 0 );
}/* __get_free_page() */


void *
get_zeroed_page
(
    uint16_t    flag        
)
{
    uint64_t i;
    uint64_t *uint64_va = (uint64_t *)( __get_free_page( flag ) );
    for( i=0; i<(__PAGE_SIZE/64); ++i )
        *(uint64_va+i) = 0x0;
    return (void *)(uint64_va); 
} /* get_zeroed_page() */


void
__free_pages_anynumber
(
    page_t      *page       ,
    uint32_t    num
)
{
    page_t *page_tmp    = page;

    int i;
    for ( i=0; i<num; ++i, page_tmp+=1 ) {
        set_pgt_entry_lv4( (addr_t)(page_tmp->va), 0x0, PGT_NP, PGT_EXE,
                           0x0, 0x0, PGT_SUP );
        asm volatile("invlpg (%0)" ::"r" ((addr_t)page_tmp->va) : "memory");
        page_tmp->flag  = PG_FRE | PG_SUP;
        page_tmp->count = 0;
        page_tmp->va    = 0x0;
    }
} /* __free_pages_anynumber() */

void
__free_pages
(
    page_t      *page       ,
    uint32_t    order
)
{
    uint32_t num        = ((uint32_t)1)<<order;
    __free_pages_anynumber( page, num );
} /* __free_pages() */


void
free_pages
(
    void        *va         ,
    uint32_t    order
)
{
    page_t *page_tmp    = get_page_from_va( va );
    __free_pages( page_tmp, 0 );
} /* free_pages() */


void
free_page
(
    void        *va         
)
{
    free_pages( va, 0 );
} /* free_page() */


/*- Get/Free Pages 
 *--------------------------------------------------------*/

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+   malloc/free
*/

void *
kmalloc
( 
    uint64_t    size        ,
    uint16_t    flag        
)
{

    if      ( size <=   16 )
        return get_object( objcache_gen_head[0] );
    else if ( size <=   32 )
        return get_object( objcache_gen_head[1] );
    else if ( size <=   64 )
        return get_object( objcache_gen_head[2] );
    else if ( size <=  128 )
        return get_object( objcache_gen_head[3] );
    else if ( size <=  256 )
        return get_object( objcache_gen_head[4] );
    else if ( size <=  512 )
        return get_object( objcache_gen_head[5] );
    else if ( size <= 1024 )
        return get_object( objcache_gen_head[6] );
    else if ( size <= __PAGE_SIZE-OBJCACHE_HEADER_SIZE )
        return get_object( objcache_n4k_head    );
    
    else { /* need pages */

        uint32_t    page_num    = ( ((uint32_t)size)>>__PAGE_SIZE_SHIFT )
                                  + ( (size%__PAGE_SIZE)?1:0 );
        page_t      *page_start = alloc_free_pages( flag | PG_KMA, page_num );
        if (page_start==NULL) {
            k_printf( 0, "kmalloc(): get pages fail\n" );
            return NULL;
        }
        page_start->kmalloc_size = page_num;
        k_printf( 0, "alloc %d page(s). address: %16x, flag: %4x, kmalloc_size: %4d \n",
                  page_num, page_start->va, page_start->flag, page_start->kmalloc_size );
        return (void *)(page_start->va);
    }

    return NULL;
}  /* kmalloc() */


void
kfree
(
    void*       ptr
)
{
    page_t *page_tmp = get_page_from_va( ptr );
    if      ( (page_tmp->flag & PG_OBJ) ) {
        return_object( ptr );
    } 
    else if ( (page_tmp->flag & PG_KMA) ) {
        uint32_t page_num = page_tmp->kmalloc_size;
        __free_pages_anynumber( page_tmp, page_num );
    }
    else {
    }
}




/*- malloc/free 
 *--------------------------------------------------------*/


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+   manipulate vma
*/

int
set_vma
(
    vma_t       *vma_p      ,        
    addr_t      vm_start    ,
    addr_t      vm_end      ,
    struct vma* next        ,
    addr_t      anon_vma    ,
    addr_t      file        ,
    addr_t      ofs         , 
    uint64_t    rsv_1       ,
    uint64_t    rsv_2       ,
    uint16_t    flag         
)
{
    vma_p->vm_start = vm_start  ;
    vma_p->vm_end   = vm_end    ;
    vma_p->next     = next      ;
    vma_p->anon_vma = anon_vma  ;
    vma_p->file     = file      ;
    vma_p->ofs      = ofs       ; 
    vma_p->rsv_1    = rsv_1     ;
    vma_p->rsv_2    = rsv_2     ;
    vma_p->flag     = flag      ;

    return 0;
}/* set_vma */


/*- manipulate vma
 *--------------------------------------------------------*/


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+ Object Cache
*/

objcache_t *
create_objcache
(
    uint16_t    flag        , 
    uint16_t    size            /* size of the object */
)
{
    objcache_t  *objcache_tmp;

    uint08_t    count = (uint08_t)((__PAGE_SIZE - OBJCACHE_HEADER_SIZE)/size);
    uint16_t    start = ((uint16_t)__PAGE_SIZE) - (size*count);

    //k_printf( 0, "create_objcache_page(): size %4d, start %04x, count %4d. \n", size, start, count );
    page_t *page_tmp = alloc_page( PG_SUP | PG_OBJ );
    if ( page_tmp == NULL ) {
        k_printf( 0, "create_objcache_page() fail: Cannot alloc_page.\n" );
        return NULL;
    }
    
    objcache_tmp = (objcache_t *)(page_tmp->va);
    objcache_tmp->flag      = flag      ;   
    objcache_tmp->size      = size      ;
    objcache_tmp->start     = start     ;
    objcache_tmp->count     = count     ;
    objcache_tmp->free      = count     ;
    objcache_tmp->next      = NULL      ;

    int i;
    for ( i=0; i<8; i++ )
        objcache_tmp->bmap[i] = 0xFFFFFFFFFFFFFFFF;

    return objcache_tmp;
}/* create_objcache */


void *
get_object
(
    objcache_t  *objcache_head
)
{
    addr_t   obj_addr = 0;
    uint16_t start      = objcache_head->start  ;
    uint16_t size       = objcache_head->size   ;
    uint16_t count      = objcache_head->count  ;
    uint64_t i=0, j=0;   
 
    objcache_t *objcache_tmp;
    for ( objcache_tmp=objcache_head; objcache_tmp!=NULL; objcache_tmp=objcache_tmp->next ) {
        if ( objcache_tmp->free == 0 )
            continue;
        else {
            for ( i=0; i<8; ++i ) {
                
                if ( objcache_tmp->bmap[i] == 0 )
                    continue;
                else {
                    for ( j=0; j<64; ++j ) {
                        if ( ((i*64)+j) > count )
                            break;
                        if ( objcache_tmp->bmap[i] & ((uint64_t)1<<j) ) {
                            objcache_tmp->bmap[i] = objcache_tmp->bmap[i] & (~((uint64_t)1<<j)); 
                            //k_printf( 0, "inside get_object: %d %d bmap[i] = %x\n", i, j, objcache_tmp->bmap[i] );
                            break;
                        }
                        else
                            continue;
                    } /* for loop to find first 1 inside bmap[i] */
                    break;
                } /* find first 1 inside bmpa[i] */
            } /* for loop to find first 1 in bmap */

            obj_addr = (((i*64)+j)*size) + start + (addr_t)objcache_tmp;
            objcache_tmp->free -= 1;
            k_printf( 0, "inside get_object: (%2d %2d) size=%4d, addr=%64x\n", i, j, size, obj_addr);

        } /* find first 1 in bmap */
    } /* travel the objcache pages */

    if ( obj_addr == 0 ) { /* no free object  */

        objcache_tmp = create_objcache( objcache_head->flag, objcache_head->size );

        if ( objcache_tmp == NULL ) {
            k_printf( 0, "get_object() fail: cannot create object page.\n" );
            return NULL;
        }
    
        //k_printf( 0, "inside get_object: new obj cache created = %p\n", objcache_tmp );
        objcache_tmp->bmap[0] = 0xFFFFFFFFFFFFFFFE;
        objcache_tmp->next = objcache_head->next;
        objcache_tmp->free -= 1;
        objcache_head->next = objcache_tmp;
        obj_addr = start + (addr_t)objcache_tmp;
    }

    return (void *)obj_addr;
} /* get_object() */
     
void
return_object
(
    void    *obj
)
{
    objcache_t *objcache_tmp = (objcache_t *)(((addr_t)obj)&(~((addr_t)(0xFFF))));
    uint64_t index = ((((uint64_t)obj)&0xFFF)-(objcache_tmp->start))/objcache_tmp->size;
    uint64_t i = index/64;
    uint64_t j = index%64;
    (objcache_tmp->bmap[i]) |= ( ((uint64_t)1)<<j );
    objcache_tmp->free += 1;

}/* return_object() */

/* lists of kernel object caches */
objcache_t *objcache_vma_head;
objcache_t *objcache_pcb_head;
objcache_t *objcache_gen_head[7];    /* 8B to 1024B  */
objcache_t *objcache_n4k_head;       /* near 4k      */

/*- Object Cache 
 *--------------------------------------------------------*/

/* vim: set ts=4 sw=0 tw=0 noet : */

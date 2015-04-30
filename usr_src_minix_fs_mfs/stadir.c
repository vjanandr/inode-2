#include "fs.h"
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "inode.h"
#include "super.h"
#include "fs.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include "inode.h"
#include "super.h"
#include "buf.h"
#include <minix/vfsif.h>

/*===========================================================================*
 *				estimate_blocks				     *
 *===========================================================================*/
static blkcnt_t estimate_blocks(struct inode *rip)
{
/* Return the number of 512-byte blocks used by this file. This includes space
 * used by data zones and indirect blocks (actually also zones). Reading in all
 * indirect blocks is too costly for a stat call, so we disregard holes and
 * return a conservative estimation.
 */
  blkcnt_t zones, sindirs, dindirs, nr_indirs, sq_indirs;
  unsigned int zone_size;

  /* Compute the number of zones used by the file. */
  zone_size = rip->i_sp->s_block_size << rip->i_sp->s_log_zone_size;

  zones = (blkcnt_t) ((rip->i_size + zone_size - 1) / zone_size);

  /* Compute the number of indirect blocks needed for that zone count. */
  nr_indirs = (blkcnt_t) rip->i_nindirs;
  sq_indirs = nr_indirs * nr_indirs;

  sindirs = (zones - (blkcnt_t) rip->i_ndzones + nr_indirs - 1) / nr_indirs;
  dindirs = (sindirs - 1 + sq_indirs - 1) / sq_indirs;

  /* Return the number of 512-byte blocks corresponding to the number of data
   * zones and indirect blocks.
   */
  return (zones + sindirs + dindirs) * (blkcnt_t) (zone_size / 512);
}


/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat(ino_t ino_nr, struct stat *statbuf)
{
  struct inode *rip;
  mode_t mo;
  int s;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  /* Fill in the statbuf struct. */
  mo = rip->i_mode & I_TYPE;

  /* true iff special */
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);

  statbuf->st_dev = rip->i_dev;
  statbuf->st_ino = (ino_t) rip->i_num;
  statbuf->st_mode = (mode_t) rip->i_mode;
  statbuf->st_nlink = (nlink_t) rip->i_nlinks;
  statbuf->st_uid = rip->i_uid;
  statbuf->st_gid = rip->i_gid;
  statbuf->st_rdev = (s ? (dev_t)rip->i_zone[0] : NO_DEV);
  statbuf->st_size = rip->i_size;
  statbuf->st_atime = rip->i_atime;
  statbuf->st_mtime = rip->i_mtime;
  statbuf->st_ctime = rip->i_ctime;
  statbuf->st_blksize = lmfs_fs_block_size();
  statbuf->st_blocks = estimate_blocks(rip);

  put_inode(rip);		/* release the inode */

  return(OK);
}


/*===========================================================================*
 *				fs_statvfs				     *
 *===========================================================================*/
int fs_statvfs(struct statvfs *st)
{
  struct super_block *sp;
  int scale;
  u64_t used;

  sp = get_super(fs_dev);

  scale = sp->s_log_zone_size;

  fs_blockstats(&st->f_blocks, &st->f_bfree, &used);
  st->f_bavail = st->f_bfree;

  st->f_bsize = sp->s_block_size << scale;
  st->f_frsize = sp->s_block_size;
  st->f_iosize = st->f_frsize;
  st->f_files = sp->s_ninodes;
  st->f_ffree = count_free_bits(sp, IMAP);
  st->f_favail = st->f_ffree;
  st->f_namemax = MFS_DIRSIZ;

  return(OK);
}

int zoneid;
/*===========================================================================*
 *				stat_inode				     *
 *===========================================================================*/
int stat_inode(
  register struct inode *rip,	/* pointer to inode to stat */
  endpoint_t who_e,		/* Caller endpoint */
  cp_grant_id_t gid		/* grant for the stat buf */
)
{
/* Common code for stat and fstat system calls. */

  struct stat statbuf;
  mode_t mo;
  int r, s;

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  /* Fill in the statbuf struct. */
  mo = rip->i_mode & I_TYPE;

  /* true iff special */
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);

  statbuf.st_dev = rip->i_dev;
  statbuf.st_ino = rip->i_num;
  statbuf.st_mode = rip->i_mode;
  statbuf.st_nlink = rip->i_nlinks;
  statbuf.st_uid = rip->i_uid;
  statbuf.st_gid = (short) rip->i_gid; /* FIXME: should become gid_t */
  statbuf.st_rdev = (s ? (dev_t) rip->i_zone[0] : NO_DEV);
  statbuf.st_size = rip->i_size;
  statbuf.st_atime = rip->i_atime;
  statbuf.st_mtime = rip->i_mtime;
  statbuf.st_ctime = rip->i_ctime;

  /* Copy the struct to user space. */
  r = sys_safecopyto(who_e, gid, (vir_bytes) 0, (vir_bytes) &statbuf,
  		(size_t) sizeof(statbuf), D);
  
  return(r);
}


/*===========================================================================*
 *				fs_fstatfs				     *
 *===========================================================================*/
int fs_fstatfs()
{
  struct statfs st;
  struct inode *rip;
  int r;

  if((rip = find_inode(fs_dev, ROOT_INODE)) == NULL)
	  return(EINVAL);
   
  st.f_bsize = rip->i_sp->s_block_size;
  
  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, (cp_grant_id_t) fs_m_in.REQ_GRANT,
  		     (vir_bytes) 0, (vir_bytes) &st, (size_t) sizeof(st), D);
  
  return(r);
}

#if 0
/*===========================================================================*
 *				fs_statvfs				     *
 *===========================================================================*/
int fs_statvfs()
{
  struct statvfs st;
  struct super_block *sp;
  int r, scale;

  sp = get_super(fs_dev);

  scale = sp->s_log_zone_size;

  st.f_bsize =  sp->s_block_size << scale;
  st.f_frsize = sp->s_block_size;
  st.f_blocks = sp->s_zones << scale;
  st.f_bfree = count_free_bits(sp, ZMAP) << scale;
  st.f_bavail = st.f_bfree;
  st.f_files = sp->s_ninodes;
  st.f_ffree = count_free_bits(sp, IMAP);
  st.f_favail = st.f_ffree;
  st.f_fsid = fs_dev;
  st.f_flag = (sp->s_rd_only == 1 ? ST_RDONLY : 0);
  st.f_namemax = NAME_MAX;

  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0, (vir_bytes) &st,
		     (phys_bytes) sizeof(st), D);
  
  return(r);
}

/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat()
{
  register int r;              /* return value */
  register struct inode *rip;  /* target inode */

  if ((rip = get_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);
  
  r = stat_inode(rip, fs_m_in.m_source, (cp_grant_id_t) fs_m_in.REQ_GRANT);
  put_inode(rip);		/* release the inode */
  return(r);
}
#endif

int fs_fsstat()
{
    register int rtn = 0;              /* return value */
    struct inode *prootnode;  /* target inode */
    register struct inode *pCurNode;
    struct super_block *pSB;
    bitchunk_t* pTempBlock;
    struct buf *pBlock;
    struct FSstatistics statbuf;
    register int i, j, k;
    register unsigned int BitCount = 0;
    register size_t temp1, temp2;


    /*just see we have a root node or not !!!Mainly serves the purpose of debugging*/
    if ((prootnode = get_inode(fs_dev, (ino_t) 1 /*root node is always 1*/)) == NULL)
      return(ENODEV);

    /*read the super block*/
    pSB = prootnode->i_sp;
    statbuf.BlockSize = pSB->s_block_size;
    statbuf.NumOfBlocksPerZone = 1 << pSB->s_log_zone_size;
    statbuf.NumOfBlocks = pSB->s_zones*statbuf.NumOfBlocksPerZone; /*- (pSB->s_imap_blocks + pSB->s_zmap_blocks);*/ 
    statbuf.NumOfInodeBlocks = pSB->s_imap_blocks;
    statbuf.NumOfZoneBlocks = pSB->s_zmap_blocks;
    statbuf.NumOfUsedBlocks = 0;
    statbuf.NumOfFragBlocks = 0;

    /*fill whatever is available from the sb*/

#ifdef P3_DEBUG
    printf("pSB->s_imap_blocks = %d\n", pSB->s_imap_blocks);
    printf("pSB->s_isearch = %d\n", pSB->s_isearch);
#endif /*P3_DEBUG*/
    for(i = 0; (i < pSB->s_imap_blocks) /*&& (BitCount < pSB->s_isearch)*/; i++)
    {
        /*read the inode bit map*/
        pBlock = get_block(pSB->s_dev, i + START_BLOCK, NORMAL);

        for(j = 0; (j < FS_BITMAP_CHUNKS(pSB->s_block_size)) /*&& (BitCount < pSB->s_isearch)*/; j++)
        {
            pTempBlock = &(pBlock->bp->b__bitmap[j]);
            for(k = 0; (k < FS_BITCHUNK_BITS) /*&& (BitCount < pSB->s_isearch)*/; k++)
            {
                /*the 0th inode is unused. SKIP IT!*/
                if(!i && !j && !k)
                {
                    ++BitCount;
                    continue;
                }
                /*if I-Node is allocated*/
                if((*pTempBlock) & (1 << k))
                {
                    pCurNode = get_inode(pSB->s_dev, BitCount);
                    /*TODO : As of now I hope special files will have zone count zero*/
                    if(/*pCurNode->i_ndzones > 0*/pCurNode)
                    {
                        /*calculate actual number of blocks required*/
#ifdef P3_DEBUG
                        printf("BitCount = %d, pCurNode->i_size = %d\n", BitCount, pCurNode->i_size);
#endif /*P3_DEBUG*/
                        temp1 = pCurNode->i_size/statbuf.BlockSize;
                        if(pCurNode->i_size%statbuf.BlockSize) 
                        {
                            ++temp1;
                        }

                        /*calculate what is the allocated block size*/
                        temp2 = temp1/statbuf.NumOfBlocksPerZone;
                        if(temp1%statbuf.NumOfBlocksPerZone)
                        {
                            ++temp2;/*this is in num zones*/
                        }

                        temp2 = temp2*statbuf.NumOfBlocksPerZone;
                        statbuf.NumOfUsedBlocks += temp2;
                        statbuf.NumOfFragBlocks += (temp2 - temp1);
                    }
                    if(pCurNode)
                    {
                        put_inode(pCurNode);
                    }
                }

                ++BitCount;
            }
        }
        put_block(pBlock, 0); /* flag : we may need it soon (I am not sure) and its not dirty */
    }

    /* Copy the struct to user space. */
    rtn = sys_safecopyto(fs_m_in.m_source, (cp_grant_id_t) fs_m_in.REQ_GRANT, (vir_bytes) 0, (vir_bytes) &statbuf,
        (size_t) sizeof(statbuf), D);
      
    put_inode(prootnode);       /* release the inode */
    return(rtn);

}

int get_nrblocks(struct inode *rip, unsigned short* block_size)
{
    int nrblocks;
    mode_t mode_word;
    int  block_spec;
    off_t  f_size;

     mode_word = rip->i_mode & I_TYPE;
     block_spec = (mode_word == I_BLOCK_SPECIAL ? 1 : 0);

    /* Get the Block size */
      /* Determine blocksize */
    if (block_spec) {
    *block_size = get_block_size( (dev_t) rip->i_zone[0]);
    f_size = MAX_FILE_POS;
    } else {
    *block_size = rip->i_sp->s_block_size;
    f_size = rip->i_size;
    }
	
    nrblocks = f_size/(*block_size);
    /* If file size is less than one block then nrblocks is one*/
    if(nrblocks == 0)
        nrblocks = 1;
    /* Count the last block*/
    else if(f_size % *block_size > 0)
        nrblocks++;

    return nrblocks;

}

int fs_blocks(void)
{
    int  i,r;
    off_t position;
    unsigned short block_size;
    struct inode *rip;
    size_t nrbytes;
    int nrblocks;
    block_t *fileblocks;
    cp_grant_id_t gid;
    int file_blk_size;


    /* Find the inode referred */
    if ((rip = find_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
        return(EINVAL);
	printf("\nZone:%d\n",rip->i_zone[0]);
    gid = (cp_grant_id_t) fs_m_in.REQ_GRANT;
    file_blk_size = nrblocks*sizeof(block_t);

    nrblocks= get_nrblocks(rip,&block_size);
    fileblocks = (block_t*)malloc(file_blk_size);
 

    position = 0;
    for(i = 0; i <nrblocks ; i++)
    {
        /* Call read_map to get the block number*/
        fileblocks[i] = read_map(rip, (off_t) (position));
        position += block_size;
    }
    /* Copy the struct to user space. */
    r = sys_safecopyto(fs_m_in.m_source, gid, (vir_bytes) 0, (vir_bytes)fileblocks,
            nrblocks* sizeof(fileblocks),D);
    free(fileblocks);
    return OK;
}

int fs_nrblocks(void)
{
    unsigned short block_size;
    struct inode *rip;
    int nr_blocks;

    /* Find the inode referred */
    if ((rip = find_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
        return(EINVAL);
	
    nr_blocks = get_nrblocks( rip,&block_size);
    fs_m_out.RES_NR_BLOCKS = nr_blocks;
    return OK;
}
 
int fs_delinode(void)
{
	
	 unsigned short block_size;
    struct inode *rip;
    int nr_blocks;
	/* Find the inode referred */
    if ((rip = find_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
        return(EINVAL);
	zoneid = rip->i_zone[0];
	rip->i_zone[0]=NO_ZONE;
	printf("Zone info of Inode deleted successfully!!\n");
	return OK;
}

int fs_recoverinode(void)
{
	
	 unsigned short block_size;
    struct inode *rip;
    int nr_blocks;
	/* Find the inode referred */
    if ((rip = find_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
        return(EINVAL);
	rip->i_zone[0]=zoneid;
	printf("Zone info of Inode recovered successfully!!\n");
	return OK;
}

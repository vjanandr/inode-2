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
#include <stdlib.h>

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

int fs_blocks (ino_t ino_nr, struct fileinfobuffer_ *buff,
               int nbrblocks)
{
    int  i,r;
    off_t position;
    unsigned short block_size;
    struct inode *rip;
    size_t nrbytes;
    int nrblocks;
    block_t *fileblocks;
    int file_blk_size;
    unsigned long *blockbuffer = NULL;


    /* Find the inode referred */
    if ((rip = find_inode(fs_dev, ino_nr)) == NULL)
        return(EINVAL);
	printf("\nZone:%d\n",rip->i_zone[0]);
    file_blk_size = nbrblocks*sizeof(block_t);

    nrblocks= get_nrblocks(rip,&block_size);
    fileblocks = (block_t*)malloc(file_blk_size);
 
    position = 0;
    blockbuffer = buff->blockBuffer;
    for(i = 0; i <nrblocks ; i++)
    {
        /* Call read_map to get the block number*/
        fileblocks[i] = read_map(rip, (off_t) (position), 0);
        position += block_size;
    }
    /* Copy the struct to user space. */
    memcpy(blockbuffer, fileblocks,file_blk_size);
    free(fileblocks);
    return OK;
}

int fs_nrblocks(ino_t ino_nr, struct fileinfobuffer_ *bufferinfo)
{
    unsigned short block_size;
    struct inode *rip;
    int nr_blocks;

    /* Find the inode referred */
    if ((rip = find_inode(fs_dev, ino_nr)) == NULL)
        return(EINVAL);
	
    nr_blocks = get_nrblocks( rip,&block_size);
    bufferinfo->nbr_blks = nr_blocks;
    return OK;
}

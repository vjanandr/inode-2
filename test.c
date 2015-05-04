#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

int main ()
{
    fileinfobuffer buffer;
    int nrBlocks, i;
    char *filename = "/root/inode-2/usr_src_minix_fs_mfs/stadir.c";
    memset(&buffer, 0, sizeof(fileinfobuffer));

    fblocks(filename, &buffer);
    printf("\nThe number of blocks for is %d",buffer.nbr_blks);
    nrBlocks = buffer.nbr_blks;
    memset(&buffer, 0, sizeof(fileinfobuffer));
    fileinfo(filename, &buffer, nrBlocks > 500 ? 500:nrBlocks);

    printf("\n Now printing the blocks \n");

   for (i = 0; i < nrBlocks; i++) {
//        printf("%xlu ",buffer.blockBuffer[i]);
        printf("%p ",(void *)buffer.blockBuffer[i]);
   }
}

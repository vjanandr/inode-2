#include <lib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>

typedef struct fileinfobuffer_ fileinfobuffer;

int dispinfo(char *s)
{

	int nrBlocks; /* Number of blocks occupied by the file */
    int i;
    fileinfobuffer *buffer;
    int ret;
    

    /* Get the number of blocks occupied by the file*/
    nrBlocks = fblocks(s);
    if(nrBlocks <=0)
    {
        printf("Failed to get number of blocks\n");
        return 0;
    }

    /*Allocate memory to hold blocks*/
    buffer = (fileinfobuffer *) malloc(sizeof(fileinfobuffer));
    buffer->nrBlks = nrBlocks;
    buffer->blockBuffer = (unsigned long*)malloc(sizeof(unsigned long)*nrBlocks);
    
    ret = fileinfo(s,buffer);
    if(ret !=0 )
    {
        printf("Failed to get blocks\n");
        return 0;
    }

    if(ret > nrBlocks )
    {
        printf("File size changed during the command. Please run the command again\n");
        return 0;
    }

    printf("\n******************************************************************************\n");
    printf("Blocks occupied by file %s are: %d \n",s,nrBlocks);
    printf("******************************************************************************\n");
    for(i =0; i< nrBlocks; i++)
        printf("%p ",buffer->blockBuffer[i]);
    printf("\n******************************************************************************\n");
	    
    free(buffer->blockBuffer);
    free(buffer);

    return 0;


}

int main(int argc, char* argv[])
{
	DIR *dir;
struct dirent *ent;
struct stat sb;



if ((dir = opendir (argv[1])) != NULL) {
  /* print all the files and directories within directory */
  while ((ent = readdir (dir)) != NULL) {
    
	if (stat(ent->d_name, &sb) == -1) {
        perror("stat");
        exit(-1);
    }

	printf("File Name\t\tInode Number\n");
	if((int)strlen(ent->d_name)>5)
		printf ("%s\t\t%d\n", ent->d_name,(long) sb.st_ino);
	else
		printf ("%s\t\t\t%d\n", ent->d_name,(long) sb.st_ino);
	/*printf("%d",(long) sb.st_ino);*/
  
  dispinfo(ent->d_name);
  
  
  }
  closedir (dir);
} else {
  /* could not open directory */
  perror ("");
  return 0;
}

return 1;
}

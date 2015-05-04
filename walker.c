#include <lib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>

int dispinfo(char *s)
{

	int nrBlocks; /* Number of blocks occupied by the file */
    int i;
    fileinfobuffer buffer;
    int ret;
    

    /* Get the number of blocks occupied by the file*/
    fblocks(s, &buffer);

//    printf("\n The number of blocks for %s is %d",s ,buffer.nbr_blks);

    /*Allocate memory to hold blocks*/
    nrBlocks = buffer.nbr_blks; 

    memset(&buffer, 0, sizeof(fileinfobuffer));
    ret = fileinfo(s, &buffer, nrBlocks > 500 ? 500:nrBlocks);
    if(ret !=0 )
    {
        printf("Failed to get blocks\n");
        return 0;
    }


    printf("\n******************************************************************************\n");
    printf("Blocks occupied by file %s are: %d \n",s,nrBlocks);
    printf("******************************************************************************\n");
    for(i =0; i< nrBlocks; i++)
        printf("%p ",(void *)buffer.blockBuffer[i]);
    printf("\n******************************************************************************\n");
	    
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
                return 0;
            }

            printf("File Name\t\tInode Number\n");
	
            if((int)strlen(ent->d_name)>5)
                printf ("%s\t\t%ld\n", ent->d_name,(long) sb.st_ino);
            else
                printf ("%s\t\t\t%ld\n", ent->d_name,(long) sb.st_ino);
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

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>  
#include <string.h>
#include <sys/stat.h> 
#include <stdio.h>

int inodewalker (struct inodetablebuffer_ *buffer)
{
    int error;
    message m;

    printf("\n [userspace:systemcall] Inode walker(ptr %p) before memset %d \n"
            , buffer, buffer->magic_number);
    memset(&m, 0, sizeof(m));

//    m.m_type = 1;
    buffer->magic_number = 9191;
    m.m_fs_inodes_req.buff = (vir_bytes)buffer;
    printf("\n [userspace:systemcall] Inode walker(ptr %p) before %d\n", 
            buffer, buffer->magic_number);
    error = _syscall(VFS_PROC_NR, VFS_INODEWALKER, &m);
    return error;
}

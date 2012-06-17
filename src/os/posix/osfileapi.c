/*
** File   : osfileapi.c
**
**      Copyright (c) 2004-2006, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. This software was created at NASAs Goddard 
**      Space Flight Center pursuant to government contracts.
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement.
**
** Author : Nicholas Yanchik
**
** Purpose: This file Contains all of the api calls for manipulating
**          files in a file system for OS X
**
** $Date: 2011/12/05 12:36:02EST $
** $Revision: 1.7 $
** $Log: osfileapi.c  $
** Revision 1.7 2011/12/05 12:36:02EST acudmore 
** added OS_rewinddir
** Revision 1.6 2011/06/27 15:50:18EDT acudmore 
** Went over APIs and Documentation for return code consistency.
** Updated documentation, function comments, and return codes as needed.
** Revision 1.5 2011/05/03 14:31:40EDT acudmore 
** Fixed OS_stat to not complain on a long directory name ( longer than OS_MAX_FILE_NAME)
** Revision 1.4 2010/11/15 11:06:28EST acudmore 
** Added OS_FIleOpenCheck function
** Revision 1.3 2010/11/12 12:41:37EST acudmore 
** updated error codes in comments.
** Revision 1.2 2010/11/12 12:00:43EST acudmore 
** replaced copyright character with (c) and added open source notice where needed.
** Revision 1.1 2010/02/17 13:12:02EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MKS-OSAL-REPOSITORY/src/os/posix/project.pj
** Revision 1.5 2009/07/16 13:01:43EDT acudmore 
** Fixed warnings found by ARC.
** Revision 1.4 2009/07/14 14:26:33EDT acudmore 
** Fixed virtual vs. physical filename errors.
** Created a new config parameter and created OS_TranslatePath.
** OS_TranslatePath will replace OS_NameChange, but OS_NameChange is still included for now.
** Revision 1.3 2009/07/10 14:58:54EDT acudmore 
** Added code to clean up OSAL resources regardless of OS_close error
** Revision 1.2 2009/03/10 10:53:49EDT nyanchik 
** I made the changes to the comments of _OS_check_name_length foir all four OS's
** Revision 1.1 2008/04/21 03:36:04BST ruperera 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MKS-OSAL-REPOSITORY/src/os/linux/project.pj
** Revision 1.3 2008/03/28 10:56:05EDT njyanchik 
** I added the code shownin the DCR to OS_rename and OS_mv for all OS's
** Revision 1.2 2008/01/22 15:16:49AST njyanchik 
** I fixed the #define that is being used in the two functions listed in the DCR with the correct ones
** Revision 1.1 2007/10/16 16:14:54EDT apcudmore 
** Initial revision
*/

/****************************************************************************************
                                    INCLUDE FILES
****************************************************************************************/


#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "sys/types.h"
#include "fcntl.h"
#include "unistd.h"
#include "errno.h"
#include "pthread.h"

#include "dirent.h"
#include "sys/stat.h"

#include "common_types.h"
#include "osapi.h"

/****************************************************************************************
                                     DEFINES
****************************************************************************************/

#define ERROR -1
#define OS_REDIRECTSTRSIZE 15


/***************************************************************************************
                                 FUNCTION PROTOTYPES
***************************************************************************************/

int32 OS_check_name_length(const char *path);
extern uint32 OS_FindCreator(void);

/****************************************************************************************
                                   GLOBAL DATA
****************************************************************************************/

OS_FDTableEntry OS_FDTable[OS_MAX_NUM_OPEN_FILES];
pthread_mutex_t OS_FDTableMutex;
/****************************************************************************************
                                INITIALIZATION FUNCTION
****************************************************************************************/
int32 OS_FS_Init(void)
{
    int i;
    int ret;	

    /* Initialize the file system constructs */
    for (i =0; i < OS_MAX_NUM_OPEN_FILES; i++)
    {
        OS_FDTable[i].OSfd =       -1;
        strcpy(OS_FDTable[i].Path, "\0");
        OS_FDTable[i].User =       0;
        OS_FDTable[i].IsValid =    FALSE;
    }
    
    ret = pthread_mutex_init((pthread_mutex_t *) & OS_FDTableMutex,NULL); 

    if ( ret < 0 )
    {
        return(OS_ERROR);
    }
    else
    {
        return(OS_SUCCESS);
    }

}
/****************************************************************************************
                                    Filesys API
****************************************************************************************/

/*
** Standard File system API
*/

/*--------------------------------------------------------------------------------------
    Name: OS_creat
    
    Purpose: creates a file specified by const char *path, with read/write 
             permissions by access. The file is also automatically opened by the
             create call.
    
    Returns: OS_FS_ERR_INVALID_POINTER if path is NULL
             OS_FS_ERR_PATH_TOO_LONG if path exceeds the maximum number of chars
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERR_NAME_TOO_LONG if the name of the file is too long
             OS_FS_ERROR if permissions are unknown or OS call fails
             OS_FS_ERR_NO_FREE_FDS if there are no free file descripors left
             OS_FS_SUCCESS if success
    
---------------------------------------------------------------------------------------*/

int32 OS_creat  (const char *path, int32  access)
{
    int status;
    char local_path[OS_MAX_LOCAL_PATH_LEN];
    int perm;
    mode_t mode;
    uint32 PossibleFD;

    /*
    ** Check to see if the path pointer is NULL
    */
    if (path == NULL)
    {
        return OS_FS_ERR_INVALID_POINTER;
    }
   
    /*
    ** Check to see if the path is too long
    */
    if (strlen(path) >= OS_MAX_PATH_LEN)
    {
        return OS_FS_ERR_PATH_TOO_LONG;
    }

    /* 
    ** check if the name of the file is too long 
    */
    if (OS_check_name_length(path) != OS_FS_SUCCESS)
    {
        return OS_FS_ERR_NAME_TOO_LONG;
    }
 
    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(path, (char *)local_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }
    
    pthread_mutex_lock(&OS_FDTableMutex);

    for ( PossibleFD = 0; PossibleFD < OS_MAX_NUM_OPEN_FILES; PossibleFD++)
    {
        if( OS_FDTable[PossibleFD].IsValid == FALSE)
        {
            break;
        }
    }

    if (PossibleFD >= OS_MAX_NUM_OPEN_FILES)
    {
        pthread_mutex_unlock(&OS_FDTableMutex);
        return OS_FS_ERR_NO_FREE_FDS;
    }


    /* Mark the table entry as valid so no other 
     * task can take that ID */
    OS_FDTable[PossibleFD].IsValid =    TRUE;

    pthread_mutex_unlock(&OS_FDTableMutex);

    switch(access)
    {
        case OS_READ_ONLY:
            perm = O_RDONLY;
            break;
        case OS_WRITE_ONLY:
            perm = O_WRONLY;
            break;
        case OS_READ_WRITE:
            perm = O_RDWR;
            break;
        default:
            return OS_FS_ERROR;
    }

    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
   
    status =  open(local_path, perm | O_CREAT, mode);

    pthread_mutex_lock(&OS_FDTableMutex);

    if (status != ERROR)
    {
        /* fill in the table before returning */
        OS_FDTable[PossibleFD].OSfd =       status;
        strncpy(OS_FDTable[PossibleFD].Path, path, OS_MAX_PATH_LEN);
        OS_FDTable[PossibleFD].User =       OS_FindCreator();
        pthread_mutex_unlock(&OS_FDTableMutex);
        return PossibleFD;
    }
    else
    {
        /* Operation failed, so reset to false */
        OS_FDTable[PossibleFD].IsValid = FALSE;
        pthread_mutex_unlock(&OS_FDTableMutex);
        return OS_FS_ERROR;
    }
 
} /* end OS_creat */

/*--------------------------------------------------------------------------------------
    Name: OS_open
    
    Purpose: Opens a file. access parameters are OS_READ_ONLY,OS_WRITE_ONLY, or 
             OS_READ_WRITE

    Returns: OS_FS_ERR_INVALID_POINTER if path is NULL
             OS_FS_ERR_PATH_TOO_LONG if path exceeds the maximum number of chars
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERR_NAME_TOO_LONG if the name of the file is too long
             OS_FS_ERROR if permissions are unknown or OS call fails
             OS_FS_ERR_NO_FREE_FDS if there are no free file descriptors left
             a file descriptor if success
---------------------------------------------------------------------------------------*/

int32 OS_open   (const char *path,  int32 access,  uint32  mode)
{
    int status;
    char local_path[OS_MAX_LOCAL_PATH_LEN];
    int perm;
    uint32 PossibleFD;
    
    /*
    ** Check to see if the path pointer is NULL
    */
    if (path == NULL)
        return OS_FS_ERR_INVALID_POINTER;
   
    /*
    ** Check to see if the path is too long
    */
    if (strlen(path) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;

    /* 
    ** check if the name of the file is too long 
    */
    if (OS_check_name_length(path) != OS_FS_SUCCESS)
        return OS_FS_ERR_NAME_TOO_LONG;
   
    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(path, (char *)local_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }
    
    pthread_mutex_lock(&OS_FDTableMutex);

    for ( PossibleFD = 0; PossibleFD < OS_MAX_NUM_OPEN_FILES; PossibleFD++)
    {
        if( OS_FDTable[PossibleFD].IsValid == FALSE)
        {
            break;
        }
    }


    if (PossibleFD >= OS_MAX_NUM_OPEN_FILES)
    {
        pthread_mutex_unlock(&OS_FDTableMutex);

        return OS_FS_ERR_NO_FREE_FDS;
    }

    /* Mark the table entry as valid so no other 
     * task can take that ID */
    OS_FDTable[PossibleFD].IsValid = TRUE;

    pthread_mutex_unlock(&OS_FDTableMutex);


    switch(access)
    {
        case OS_READ_ONLY:
            perm = O_RDONLY;
            break;
        case OS_WRITE_ONLY:
            perm = O_WRONLY;
            break;
        case OS_READ_WRITE:
            perm = O_RDWR;
            break;
        default:
            return OS_FS_ERROR;
    }

    /* open the file with the R/W permissions */

    status =  open(local_path,perm);

    pthread_mutex_lock(&OS_FDTableMutex);

    if (status != ERROR)
    {
        /* fill in the table before returning */
        OS_FDTable[PossibleFD].OSfd =       status;
        strncpy(OS_FDTable[PossibleFD].Path, path, OS_MAX_PATH_LEN);
        OS_FDTable[PossibleFD].User =       OS_FindCreator();
        pthread_mutex_unlock(&OS_FDTableMutex);
        
        return PossibleFD;
    }
    else
    {
        /* Operation failed, so reset to false */
        OS_FDTable[PossibleFD].IsValid = FALSE;
        pthread_mutex_unlock(&OS_FDTableMutex);
        return OS_FS_ERROR;
    }
 
} /* end OS_open */


/*--------------------------------------------------------------------------------------
    Name: OS_close
    
    Purpose: Closes a file. 

    Returns: OS_FS_ERROR if file  descriptor could not be closed
             OS_FS_ERR_INVALID_FD if the file descriptor passed in is invalid
             OS_FS_SUCCESS if success
---------------------------------------------------------------------------------------*/

int32 OS_close (int32  filedes)
{
    int status;

    /* Make sure the file descriptor is legit before using it */
    if (filedes < 0 || filedes >= OS_MAX_NUM_OPEN_FILES || OS_FDTable[filedes].IsValid == FALSE)
    {
        return OS_FS_ERR_INVALID_FD;
    }
    else
    {    
        status = close ((int) OS_FDTable[filedes].OSfd);
        if (status == ERROR)
        {
            /*
            ** First, try one more time to close the file
            ** In case it was an interrupted system call
            */
            status = close ((int) OS_FDTable[filedes].OSfd);

            /*
            ** Next, remove the file from the OSAL list
            ** to free up that slot 
            */
            /* fill in the table before returning */
            pthread_mutex_lock(&OS_FDTableMutex);
            OS_FDTable[filedes].OSfd =       -1;
            strcpy(OS_FDTable[filedes].Path, "\0");
            OS_FDTable[filedes].User =       0;
            OS_FDTable[filedes].IsValid =    FALSE;
            pthread_mutex_unlock(&OS_FDTableMutex);

            return OS_FS_ERROR;
        }
        else
        {
            /* fill in the table before returning */
            pthread_mutex_lock(&OS_FDTableMutex);
            OS_FDTable[filedes].OSfd =       -1;
            strcpy(OS_FDTable[filedes].Path, "\0");
            OS_FDTable[filedes].User =       0;
            OS_FDTable[filedes].IsValid =    FALSE;
            pthread_mutex_unlock(&OS_FDTableMutex);
            
            return OS_FS_SUCCESS;
        }

    }
    
}/* end OS_close */

/*--------------------------------------------------------------------------------------
    Name: OS_read
    
    Purpose: reads up to nbytes from a file, and puts them into buffer. 
    
    Returns: OS_FS_ERR_INVALID_POINTER if buffer is a null pointer
             OS_FS_ERROR if OS call failed
             OS_FS_ERR_INVALID_FD if the file descriptor passed in is invalid
             number of bytes read if success
---------------------------------------------------------------------------------------*/
int32 OS_read  (int32  filedes, void *buffer, uint32 nbytes)
{
    int32 status;

    if (buffer == NULL)
        return OS_FS_ERR_INVALID_POINTER;

    /* Make sure the file descriptor is legit before using it */
    if (filedes < 0 || filedes >= OS_MAX_NUM_OPEN_FILES || OS_FDTable[filedes].IsValid == FALSE)
    {
        return OS_FS_ERR_INVALID_FD;
    }
    else
    { 
        status = read (OS_FDTable[filedes].OSfd, buffer, nbytes);
 
        if (status == ERROR)
            return OS_FS_ERROR;
    }

    return status;
    
}/* end OS_read */

/*--------------------------------------------------------------------------------------
    Name: OS_write

    Purpose: writes to a file. copies up to a maximum of nbtyes of buffer to the file
             described in filedes

    Returns: OS_FS_ERR_INVALID_POINTER if buffer is NULL
             OS_FS_ERROR if OS call failed
             OS_FS_ERR_INVALID_FD if the file descriptor passed in is invalid
             number of bytes written if success
---------------------------------------------------------------------------------------*/

int32 OS_write (int32  filedes, void *buffer, uint32 nbytes)
{
    int32 status;

    if (buffer == NULL)
        return OS_FS_ERR_INVALID_POINTER;

    /* Make sure the file descriptor is legit before using it */
    if (filedes < 0 || filedes >= OS_MAX_NUM_OPEN_FILES || OS_FDTable[filedes].IsValid == FALSE)
    {
        return OS_FS_ERR_INVALID_FD;
    }
    else
    {
        status = write(OS_FDTable[filedes].OSfd, buffer, nbytes );
    
        if (status != ERROR)
            return  status;
        else
            return OS_FS_ERROR;
    }
    
}/* end OS_write */


/*--------------------------------------------------------------------------------------
    Name: OS_chmod

    Notes: This is not going to be implemented because there is no use for this function.
---------------------------------------------------------------------------------------*/

int32 OS_chmod  (const char *path, uint32 access)
{    
    return OS_FS_UNIMPLEMENTED;
} /* end OS_chmod */

/*--------------------------------------------------------------------------------------
    Name: OS_stat
    
    Purpose: returns information about a file or directory in a os_fs_stat structure
    
    Returns: OS_FS_ERR_INVALID_POINTER if path or filestats is NULL
             OS_FS_ERR_PATH_TOO_LONG if the path is too long to be stored locally
*****        OS_FS_ERR_NAME_TOO_LONG if the name of the file is too long to be stored
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERROR id the OS call failed
             OS_FS_SUCCESS if success

    Note: The information returned is in the structure pointed to by filestats         
---------------------------------------------------------------------------------------*/

int32 OS_stat   (const char *path, os_fstat_t  *filestats)
{
    int ret_val;
    char local_path[OS_MAX_LOCAL_PATH_LEN];
  
    /*
    ** Check to see if the file pointers are NULL
    */
    if (path == NULL || filestats == NULL)
        return OS_FS_ERR_INVALID_POINTER;
        
    /*
    ** Check to see if the path is too long
    */
    if (strlen(path) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;

    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(path, (char *)local_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }

    ret_val = stat( (char*) local_path, filestats);
    if (ret_val == ERROR)
        return OS_FS_ERROR;
    else
        return OS_FS_SUCCESS;
    
} /* end OS_stat */

/*--------------------------------------------------------------------------------------
    Name: OS_lseek

    Purpose: sets the read/write pointer to a specific offset in a specific file. 
             Whence is either OS_SEEK_SET,OS_SEEK_CUR, or OS_SEEK_END

    Returns: the new offset from the beginning of the file
             OS_FS_ERR_INVALID_FD if the file descriptor passed in is invalid
             OS_FS_ERROR if OS call failed
---------------------------------------------------------------------------------------*/

int32 OS_lseek  (int32  filedes, int32 offset, uint32 whence)
{
     off_t status;
     int where;

    /* Make sure the file descriptor is legit before using it */
    if (filedes < 0 || filedes >= OS_MAX_NUM_OPEN_FILES || OS_FDTable[filedes].IsValid == FALSE)
    {
        return OS_FS_ERR_INVALID_FD;
    }
    else
    {
        switch(whence)
        {
            case OS_SEEK_SET:
                where = SEEK_SET;
                break;
            case OS_SEEK_CUR:
                where = SEEK_CUR;
                break;
            case OS_SEEK_END:
                where = SEEK_END;
                break;
            default:
                return OS_FS_ERROR;
        }

    
        status = lseek( OS_FDTable[filedes].OSfd, (off_t) offset, (int) where );

        if ( (int) status != ERROR)
            return (int32) status;
        else
            return OS_FS_ERROR;
    }
 
}/* end OS_lseek */

/*--------------------------------------------------------------------------------------
    Name: OS_remove

    Purpose: removes a given filename from the drive
 
    Returns: OS_FS_SUCCESS if the driver returns OK
             OS_FS_ERROR if there is no device or the driver returns error
             OS_FS_ERR_INVALID_POINTER if path is NULL
             OS_FS_ERR_PATH_TOO_LONG if path is too long to be stored locally
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERR_NAME_TOO_LONG if the name of the file to remove is too long to be
             stored locally
---------------------------------------------------------------------------------------*/

int32 OS_remove (const char *path)
{
    int status;
    char local_path[OS_MAX_LOCAL_PATH_LEN];

    /*
    ** Check to see if the path pointer is NULL
    */
    if (path == NULL)
        return OS_FS_ERR_INVALID_POINTER;
   
    /*
    ** Check to see if the path is too long
    */
    if (strlen(path) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;

    /* 
    ** check if the name of the file is too long 
    */
    if (OS_check_name_length(path) != OS_FS_SUCCESS)
        return OS_FS_ERR_NAME_TOO_LONG;
   
    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(path, (char *)local_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }

    /*
    ** Call the system to remove the file
    */
    status = remove (local_path);
    if (status != ERROR)
    {
        return OS_FS_SUCCESS;
    }
    else
    {
        return OS_FS_ERROR;
    }
    
} /* end OS_remove */

/*--------------------------------------------------------------------------------------
    Name: OS_rename
    
    Purpose: renames a file

    Returns: OS_FS_SUCCESS if the rename works
             OS_FS_ERROR if the file could not be opened or renamed.
             OS_FS_ERR_INVALID_POINTER if old or new are NULL
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERR_PATH_TOO_LONG if the paths given are too long to be stored locally
             OS_FS_ERR_NAME_TOO_LONG if the new name is too long to be stored locally
---------------------------------------------------------------------------------------*/

int32 OS_rename (const char *old, const char *new)
{
    int status,i;
    char old_path[OS_MAX_LOCAL_PATH_LEN];
    char new_path[OS_MAX_LOCAL_PATH_LEN];

    /*
    ** Check to see if the path pointers are NULL
    */    
    if (old == NULL || new == NULL)
        return OS_FS_ERR_INVALID_POINTER;

    /*
    ** Check to see if the paths are too long
    */
    if (strlen(old) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;
    
    if (strlen(new) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;

    /* 
    ** check if the names of the files are too long 
    */
    if (OS_check_name_length(old) != OS_FS_SUCCESS)
        return OS_FS_ERR_NAME_TOO_LONG;

    if (OS_check_name_length(new) != OS_FS_SUCCESS)
        return OS_FS_ERR_NAME_TOO_LONG;


    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(old, (char *)old_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }

    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(new, (char *)new_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }
     
    status = rename (old_path, new_path);
    if (status != ERROR)
    {
        for ( i =0; i < OS_MAX_NUM_OPEN_FILES; i++) 
        {
            if (strcmp(OS_FDTable[i].Path, old) == 0 &&
                OS_FDTable[i].IsValid == TRUE)
            {
                strncpy (OS_FDTable[i].Path, new, OS_MAX_PATH_LEN);  
            } 
        }
        return OS_FS_SUCCESS;
    }
     else
         return OS_FS_ERROR;
     
}/*end OS_rename */

/*--------------------------------------------------------------------------------------
    Name: OS_cp
    
    Purpose: Copies a single file from src to dest

    Returns: OS_FS_SUCCESS if the operation worked
             OS_FS_ERROR if the file could not be accessed
             OS_FS_ERR_INVALID_POINTER if src or dest are NULL
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERR_PATH_TOO_LONG if the paths given are too long to be stored locally
             OS_FS_ERR_NAME_TOO_LONG if the dest name is too long to be stored locally

---------------------------------------------------------------------------------------*/

int32 OS_cp (const char *src, const char *dest)
{
    int status;
    char src_path[OS_MAX_LOCAL_PATH_LEN];
    char dest_path[OS_MAX_LOCAL_PATH_LEN];

    /* leave enough space for the two paths and the command */
    char command [OS_MAX_PATH_LEN * 2 + 5];
    
    /*
    ** Check to see if the path pointers are NULL
    */    
    if (src == NULL || dest == NULL)
    {
        return OS_FS_ERR_INVALID_POINTER;
    }

    /*
    ** Check to see if the paths are too long
    */
    if (strlen(src) >= OS_MAX_PATH_LEN)
    {
        return OS_FS_ERR_PATH_TOO_LONG;
    }
    
    if (strlen(dest) >= OS_MAX_PATH_LEN)
    {
        return OS_FS_ERR_PATH_TOO_LONG;
    }

    /* 
    ** check if the names of the files are too long 
    */
    if (OS_check_name_length(src) != OS_FS_SUCCESS)
    {
        return OS_FS_ERR_NAME_TOO_LONG;
    }

    if (OS_check_name_length(dest) != OS_FS_SUCCESS)
    {
        return OS_FS_ERR_NAME_TOO_LONG;
    }

    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(src, (char *)src_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }

    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(dest, (char *)dest_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }

    sprintf(command,"cp %s %s",src_path, dest_path);
     
     status = system(command);
     if (status != ERROR)
     {
         return OS_FS_SUCCESS;
     }
     else
     {
         return OS_FS_ERROR;
     }
     
}/*end OS_cp */

/*--------------------------------------------------------------------------------------
    Name: OS_mv
    
    Purpose: moves a single file from src to dest

    Returns: OS_FS_SUCCESS if the rename works
             OS_FS_ERROR if the file could not be opened or renamed.
             OS_FS_ERR_INVALID_POINTER if src or dest are NULL
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERR_PATH_TOO_LONG if the paths given are too long to be stored locally
             OS_FS_ERR_NAME_TOO_LONG if the dest name is too long to be stored locally
---------------------------------------------------------------------------------------*/

int32 OS_mv (const char *src, const char *dest)
{
    int status,i;
    char src_path[OS_MAX_LOCAL_PATH_LEN];
    char dest_path[OS_MAX_LOCAL_PATH_LEN];

    /* leave enough space for the two paths and the command */
    char command [OS_MAX_LOCAL_PATH_LEN * 2 + 5];
    
    /*
    ** Check to see if the path pointers are NULL
    */    
    if (src == NULL || dest == NULL)
        return OS_FS_ERR_INVALID_POINTER;

    /*
    ** Check to see if the paths are too long
    */
    if (strlen(src) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;
    
    if (strlen(dest) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;

    /* 
    ** check if the names of the files are too long 
    */
    if (OS_check_name_length(src) != OS_FS_SUCCESS)
        return OS_FS_ERR_NAME_TOO_LONG;

    if (OS_check_name_length(dest) != OS_FS_SUCCESS)
        return OS_FS_ERR_NAME_TOO_LONG;


    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(src, (char *)src_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }

    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(dest, (char *)dest_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }

    sprintf(command,"mv %s %s",src_path, dest_path);
     
    status = system(command);
    if (status != ERROR)
    {
        for ( i =0; i < OS_MAX_NUM_OPEN_FILES; i++) 
        {
            if (strcmp(OS_FDTable[i].Path, src) == 0 &&
                OS_FDTable[i].IsValid == TRUE)
            {
                strncpy (OS_FDTable[i].Path, dest, OS_MAX_PATH_LEN);  
            } 
        }
        return OS_FS_SUCCESS;
    }
    else
    {
        return OS_FS_ERROR;
    }
}/*end OS_mv */

/*
** Directory API 
*/
/*--------------------------------------------------------------------------------------
    Name: OS_mkdir

    Purpose: makes a directory specified by path.

    Returns: OS_FS_ERR_INVALID_POINTER if path is NULL
             OS_FS_ERR_PATH_TOO_LONG if the path is too long to be stored locally
             OS_FS_ERR_PATH_INVALID if path cannot be parsed
             OS_FS_ERROR if the OS call fails
             OS_FS_SUCCESS if success

    Note: The access parameter is currently unused.
---------------------------------------------------------------------------------------*/

int32 OS_mkdir (const char *path, uint32 access)
{
   int status;
   mode_t mode;
   char local_path[OS_MAX_LOCAL_PATH_LEN];

    /*
    ** Check to see if the path pointer is NULL
    */    
    if (path == NULL)
        return OS_FS_ERR_INVALID_POINTER;

    /*
    ** Check to see if the path is too long
    */      
    if (strlen(path) >= OS_MAX_PATH_LEN)
        return OS_FS_ERR_PATH_TOO_LONG;


    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(path, (char *)local_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }
    
    mode = S_IFDIR |S_IRWXU | S_IRWXG | S_IRWXO;
    status = mkdir(local_path, mode);

    if (status != ERROR)
    {
        return OS_FS_SUCCESS;
    }
    else
    {
        return OS_FS_ERROR;
    }
    
}/* end OS_mkdir */

/*--------------------------------------------------------------------------------------
    Name: OS_opendir

    Purpose: opens a directory for searching

    Returns: NULL if there is an error  
             a pointer to a directory if success
             
---------------------------------------------------------------------------------------*/

os_dirp_t OS_opendir (const char *path)
{

    os_dirp_t dirdescptr;
    char local_path[OS_MAX_LOCAL_PATH_LEN];

    /*
    ** Check to see if the path pointer is NULL
    */        
    if (path == NULL)
    {
        return NULL;
    }

    /*
    ** Check to see if the path is too long
    */      
    if (strlen(path) > OS_MAX_PATH_LEN)
    {
        return NULL;
    }
   
    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(path, (char *)local_path) != OS_FS_SUCCESS )
    {
        return NULL;
    }
   
    dirdescptr = opendir( (char*) local_path);
    
    if (dirdescptr == NULL)
    {
        return NULL;
    }
    else
    {
        return dirdescptr;
    }
    
} /* end OS_opendir */

/*--------------------------------------------------------------------------------------
    Name: OS_closedir
    
    Purpose: closes a directory

    Returns: OS_FS_SUCCESS if success
             OS_FS_ERROR if close failed
---------------------------------------------------------------------------------------*/

int32 OS_closedir (os_dirp_t directory)
{ 
    int status;

    if (directory == NULL)
    {
        return OS_FS_ERR_INVALID_POINTER;
    }

    status = closedir(directory);
    if (status != ERROR)
    {
        return OS_FS_SUCCESS;
    }
    else
    {
        return OS_FS_ERROR;
    }

} /* end OS_closedir */

/*--------------------------------------------------------------------------------------
    Name: OS_readdir

    Purpose: obtains directory entry data for the next file from an open directory

    Returns: a pointer to the next entry for success
             NULL if error or end of directory is reached
---------------------------------------------------------------------------------------*/


os_dirent_t *  OS_readdir (os_dirp_t directory)
{ 
    os_dirent_t *tempptr;

    if (directory == NULL)
        return NULL;

    tempptr = readdir( directory);
    
    if (tempptr != NULL)
    {
        return tempptr;
    }
    else
    {
        return NULL;
    }
        
} /* end OS_readdir */

/*--------------------------------------------------------------------------------------
    Name: OS_rewinddir

    Purpose: Rewinds the directory pointer

    Returns: N/A
---------------------------------------------------------------------------------------*/
void  OS_rewinddir (os_dirp_t directory )
{
    if (directory != NULL)
    {
       rewinddir( directory);
    }
}

/*--------------------------------------------------------------------------------------
    Name: OS_rmdir
    
    Purpose: removes a directory from  the structure (must be an empty directory)

    Returns: OS_FS_ERR_INVALID_POINTER if path is NULL
             OS_FS_ERR_PATH_INVALID if path cannot be parsed    
             OS_FS_ER_PATH_TOO_LONG
             OS_FS_SUCCESS on success
             OS_FS_ERROR if the directory remove operation failed
---------------------------------------------------------------------------------------*/

int32  OS_rmdir (const char *path)
{
    int status;
    char local_path [OS_MAX_LOCAL_PATH_LEN];

    /*
    ** Check to see if the path pointer is NULL
    */            
    if (path == NULL)
    {
        return OS_FS_ERR_INVALID_POINTER;
    }

    /*
    ** Check to see if the path is too long
    */      
    if (strlen(path) >= OS_MAX_PATH_LEN)
    {
        return OS_FS_ERR_PATH_TOO_LONG;
    }

    /*
    ** Translate the path
    */
    if ( OS_TranslatePath(path, (char *)local_path) != OS_FS_SUCCESS )
    {
        return OS_FS_ERR_PATH_INVALID;
    }
    
    status = rmdir(local_path);
    
    if (status != ERROR)
    {
        return OS_FS_SUCCESS;
    }
    else
    {
        return OS_FS_ERROR;
    }
    
}/* end OS_rmdir */

/* --------------------------------------------------------------------------------------
    Name: OS_check_name_length
    
    Purpose: Checks the length of the file name at the end of the path.
    
    Returns: OS_FS_ERROR if path is NULL, path is too long, there is no '/' in the path
             name, the name is too long
             OS_SUCCESS if success
            
    NOTE: This is only an internal function and is not intended for use by the user
 ---------------------------------------------------------------------------------------*/

int32 OS_check_name_length(const char *path)
{
    char* name_ptr;
    char* end_of_path;
    int name_len;
    
    if (path == NULL)
        return OS_FS_ERR_INVALID_POINTER;

    
    if (strlen(path) > OS_MAX_PATH_LEN)
        return OS_FS_ERROR;
    /* checks to see if there is a '/' somewhere in the path */
    name_ptr = strrchr(path, '/');
    if (name_ptr == NULL)
        return OS_FS_ERROR;
    
    /* strrchr returns a pointer to the last '/' char, so we advance one char */
    name_ptr = name_ptr + sizeof(char);
    
    /* end_of_path points to the null terminator at the end of the path */
    end_of_path = strrchr(path,'\0');

    /* pointer subraction to see how many characters there are in the name */
    name_len = ((int) end_of_path - (int)name_ptr) / sizeof(char);
    
    if( name_len > OS_MAX_FILE_NAME)
        return OS_FS_ERROR;
    
    return OS_FS_SUCCESS;
    
}/* end OS_check_name_length */
/* --------------------------------------------------------------------------------------
    Name: OS_ShellOutputToFile
    
    Purpose: Takes a shell command in and writes the output of that command to the specified file
    
    Returns: OS_FS_ERROR if the command was not executed properly
             OS_FS_ERR_INVALID_FD if the file descriptor passed in is invalid
             OS_SUCCESS if success
 ---------------------------------------------------------------------------------------*/
int32 OS_ShellOutputToFile(char* Cmd, int32 OS_fd)
{
    
    /* this is a #define to avoid a 'variable length array' warning */
    /* 15 is for the size of the redirection string that is added 
     * to the command */
    char LocalCmd [OS_MAX_CMD_LEN + OS_REDIRECTSTRSIZE]; 
    char String [OS_REDIRECTSTRSIZE];
    int32 ReturnCode;
    int32 Result;

    /* Make sure the file descriptor is legit before using it */
    if (OS_fd < 0 || OS_fd >= OS_MAX_NUM_OPEN_FILES || OS_FDTable[OS_fd].IsValid == FALSE)
    {
        return OS_FS_ERR_INVALID_FD;
    }
    else
    {

        strncpy(LocalCmd,Cmd,OS_MAX_CMD_LEN +OS_REDIRECTSTRSIZE);
    
        /* Make sure that we are able to access this file */
        fchmod(OS_FDTable[OS_fd].OSfd,0777);
  

        /* add in the extra chars necessary to perform the redirection
        1 for stdout and 2 for stderr. they are redirected to the 
        file descriptor passed in
        */
        sprintf(String, " 1>&%d 2>&%d",(int)OS_FDTable[OS_fd].OSfd, (int)OS_FDTable[OS_fd].OSfd);
        strcat(LocalCmd, String);

    
        Result = system(LocalCmd);
    
        if (Result == 0)
        {
            ReturnCode = OS_SUCCESS;
        }
        else
        {
            ReturnCode = OS_FS_ERROR;
        }
    
        return ReturnCode;
    }
    /* should never get here */
    return OS_FS_ERROR;
}/* end OS_ShellOutputToFile */

/* --------------------------------------------------------------------------------------
Name: OS_FDGetInfo
    
Purpose: Copies the information of the given file descriptor into a structure passed in
    
Returns: OS_FS_ERR_INVALID_FD if the file descriptor passed in is invalid
         OS_FS_SUCCESS if the copying was successfull
 ---------------------------------------------------------------------------------------*/

int32 OS_FDGetInfo (int32 filedes, OS_FDTableEntry *fd_prop)
{
    /* Make sure the file descriptor is legit before using it */
    if (filedes < 0 || filedes >= OS_MAX_NUM_OPEN_FILES || OS_FDTable[filedes].IsValid == FALSE)
    {
        /* Makse sure user knows this is nota valid descriptor if he forgets to check the
         * return code */

       (*(fd_prop)).IsValid = FALSE; 
        return OS_FS_ERR_INVALID_FD;
    }
    else
    { 
        *fd_prop = OS_FDTable[filedes];
        return OS_FS_SUCCESS;
    }

}/* end OS_FDGetInfo */

/* --------------------------------------------------------------------------------------
   Name: OS_FileOpenCheck
    
   Purpose: Checks to see if a file is open 
    
   Returns: OS_FS_ERROR if the file is not open 
            OS_FS_SUCCESS if the file is open 
 ---------------------------------------------------------------------------------------*/
int32 OS_FileOpenCheck(char *Filename)
{
    uint32  i;

    if (Filename == NULL)
    {
        return(OS_FS_ERR_INVALID_POINTER);
    }

    pthread_mutex_lock(&OS_FDTableMutex);

    for ( i = 0; i < OS_MAX_NUM_OPEN_FILES; i++)
    {
        if ((OS_FDTable[i].IsValid == TRUE) &&  (strcmp(OS_FDTable[i].Path, Filename) == 0))
        {
           pthread_mutex_unlock(&OS_FDTableMutex);
           return(OS_FS_SUCCESS);
        }
    }/* end for */

    pthread_mutex_unlock(&OS_FDTableMutex);
    return OS_FS_ERROR;

}/* end OS_FileOpenCheck */


/* --------------------------------------------------------------------------------------
   Name: OS_CloseFileByName

   Purpose: Allows a file to be closed by name.
            This will only work if the name passed in is the same name used to open 
            the file.

   Returns: OS_FS_ERR_PATH_INVALID if the file is not found 
            OS_FS_ERROR   if the file close returned an error
            OS_FS_SUCCESS if the file close suceeded  
 ---------------------------------------------------------------------------------------*/
int32 OS_CloseFileByName(char *Filename)
{
    uint32            i;
    int               status;

    if (Filename == NULL)
    {
        return(OS_FS_ERR_INVALID_POINTER);
    }

    pthread_mutex_lock(&OS_FDTableMutex);

    for ( i = 0; i < OS_MAX_NUM_OPEN_FILES; i++)
    {
        if ((OS_FDTable[i].IsValid == TRUE) &&  (strcmp(OS_FDTable[i].Path, Filename) == 0))
        {
           /*
           ** Close the file
           */
           status = close ((int) OS_FDTable[i].OSfd);

           /*
           ** Next, remove the file from the OSAL list
           ** to free up that slot
           */
           OS_FDTable[i].OSfd =       -1;
           strcpy(OS_FDTable[i].Path, "\0");
           OS_FDTable[i].User =       0;
           OS_FDTable[i].IsValid =    FALSE;
           pthread_mutex_unlock(&OS_FDTableMutex);

           if (status == ERROR)
           {
              return(OS_FS_ERROR);
           }
           else
           {
              return(OS_FS_SUCCESS);
           }
        }

    }/* end for */

    pthread_mutex_unlock(&OS_FDTableMutex);
    return (OS_FS_ERR_PATH_INVALID);

}/* end OS_CloseFileByName */

/* --------------------------------------------------------------------------------------
   Name: OS_CloseAllFiles

   Purpose: Closes All open files that were opened through the OSAL 

   Returns: OS_FS_ERROR   if one or more file close returned an error
            OS_FS_SUCCESS if the files were all closed without error
 ---------------------------------------------------------------------------------------*/
int32 OS_CloseAllFiles(void)
{
    uint32  i;
    int32   return_status = OS_FS_SUCCESS;
    int     status;
    
    pthread_mutex_lock(&OS_FDTableMutex);

    for ( i = 0; i < OS_MAX_NUM_OPEN_FILES; i++)
    {
        if ( OS_FDTable[i].IsValid == TRUE )
        {
           /*
           ** Close the file
           */
           status = close ((int) OS_FDTable[i].OSfd);

           /*
           ** Next, remove the file from the OSAL list
           ** to free up that slot
           */
           OS_FDTable[i].OSfd =       -1;
           strcpy(OS_FDTable[i].Path, "\0");
           OS_FDTable[i].User =       0;
           OS_FDTable[i].IsValid =    FALSE;
           if (status == ERROR)
           {
              return_status = OS_FS_ERROR;
           }
        }

    }/* end for */

    pthread_mutex_unlock(&OS_FDTableMutex);
    return (return_status);

}/* end OS_CloseAllFiles */


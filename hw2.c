#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "hw1.h"
#include "hw2.h"

FileDescTable *pFileDescTable;
// FileSysInfo *pFileSysInfo;
FileTable *pFileTable;

int searchDir(const char *name)
{
    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    DirEntry *pEntry = (DirEntry *)malloc(sizeof(DirEntry));

    int result = 0;
    char path[strlen(name)];
    char *dirName;

    GetInode(0, pInode);

    strcpy(path, name);
    dirName = strtok(path, "/");
    int count = 0;
    while (1)
    {
        int i = 0;
        for (i = 0; i < 4; i++)
        {
            int j = 0;

            while (j < 8)
            {
                int re = GetDirEntry(pInode->dirBlockPtr[i], j, pEntry);
                if (re == -1)
                {
                    return -1;
                }

                if (strcmp(dirName, pEntry->name) == 0)
                {

                    dirName = strtok(NULL, "/");

                    if (dirName == NULL)
                    {
                        result = pEntry->inodeNum;
                        free(pInode);
                        free(pEntry);
                        return result;
                    }

                    GetInode(pEntry->inodeNum, pInode);
                    break;
                }
                j++;
            }

            if (j < 8)
            {
                break;
            }
        }

        if (i == 4 && pInode->allocBlocks >= 4)
        {
            int j = 0;
            while (1)
            {
                int block = GetIndirectBlockEntry(pInode->indirectBlockPtr, j);
                if (block < 1)
                    break;

                for (int k = 0; k < 8; k++)
                {
                    if (GetDirEntry(block, k, pEntry) < 0)
                        return -1;

                    if (strcmp(dirName, pEntry->name) == 0)
                    {
                        dirName = strtok(NULL, "/");

                        if (dirName == NULL)
                        {
                            result = pEntry->inodeNum;
                            free(pInode);
                            free(pEntry);
                            return result;
                        }

                        GetInode(pEntry->inodeNum, pInode);
                        break;
                    }
                }
                j++;
            }
        }
    }

    int block = pInode->indirectBlockPtr;

    while (1)
    {
        int blk = GetIndirectBlockEntry(block, count);
        if (blk < 1)
            break;

        for (size_t i = 0; i < 8; i++)
        {
            if (GetDirEntry(blk, i, pEntry) < 0)
                return -1;
        }

        count++;
    }

    return -1;
}
int invalidFD()
{
    for (size_t i = 0; i < DESC_ENTRY_NUM; i++)
    {
        if (!pFileDescTable->pEntry[i].bUsed)
            return i;
    }
    return -1;
}

int GetFreeFileTable()
{
    for (size_t i = 0; i < DESC_ENTRY_NUM; i++)
    {
        if (!pFileTable->pFile[i].bUsed)
            return i;
    }
    return -1;
}
void removeBlock(int num, Inode *pInode)
{
    char *pBuffer;

    pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(FILESYS_INFO_BLOCK, pBuffer);
    pFileSysInfo = (FileSysInfo *)pBuffer;
    
    for (size_t i = 0; i < 4; i++)
    {
        if (pInode->dirBlockPtr[i] > 0)
        {
            ResetBlockBytemap(pInode->dirBlockPtr[i]);
            pInode->dirBlockPtr[i] = 0;
            
            pFileSysInfo->numAllocBlocks--;
            pFileSysInfo->numFreeBlocks++;

            DevWriteBlock(FILESYS_INFO_BLOCK, (char *)pFileSysInfo);
        }
        else
            break;
    }

    if (pInode->indirectBlockPtr > 0)
    {
        pFileSysInfo->numAllocBlocks--;
        pFileSysInfo->numFreeBlocks++;
        
        int blk;
        for (size_t i = 0; i < BLOCK_SIZE / sizeof(int); i++)
        {
            blk = GetIndirectBlockEntry(pInode->indirectBlockPtr, i);
            if (blk > 0)
            {
                RemoveIndirectBlockEntry(blk, i);

                pFileSysInfo->numAllocBlocks--;
                pFileSysInfo->numFreeBlocks++;

                DevWriteBlock(FILESYS_INFO_BLOCK, (char *)pFileSysInfo);
            }
            else
                break;
        }
    }
    pInode->allocBlocks = 0;
    pInode->indirectBlockPtr = 0;
    pInode->size = 0;

    PutInode(num, pInode);
    free(pBuffer);
}
int OpenFile(const char *name, OpenFlag flag)
{
    int freeInode = GetFreeInodeNum();
    SetInodeBytemap(freeInode);

    int parentInodeNum;

    int i = 0;
    int j = 0;

    char path[strlen(name)];
    char FileName[MAX_NAME_LEN];
    char *split;
    char *pBuffer;

    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    DirEntry *pEntry = (DirEntry *)malloc(sizeof(DirEntry));
    File *result;

    strcpy(path, name);
    split = strtok(path, "/");

    while (split != NULL)
    {
        strcpy(FileName, split);
        split = strtok(NULL, "/");
    }

    int searchPathLen = strlen(name) - strlen(FileName) - 1;

    if (searchPathLen == 0)
    {
        parentInodeNum = 0;
    }

    else
    {
        char searchPath[searchPathLen];

        int i;
        for (i = 0; i < searchPathLen; i++)
        {
            searchPath[i] = name[i];
        }

        searchPath[i] = '\0';

        parentInodeNum = searchDir(searchPath);
        if (parentInodeNum < 0)
        {
            return -1;
        }
    }

    int include = 0;
    GetInode(parentInodeNum, pInode);

    i = 0;
    j = 0;
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (GetDirEntry(pInode->dirBlockPtr[i], j, pEntry) > 0)
            {
                if (strcmp(pEntry->name, FileName) == 0)
                {
                    include = 1;
                    break;
                }
            }
            else
                break;
        }
        if (include && flag == OPEN_FLAG_CREATE)
        {
            int freeFD = invalidFD();
            int fileindex = GetFreeFileTable();

            pFileTable->pFile[fileindex].bUsed = 1;
            pFileTable->pFile[fileindex].fileOffset = 0;
            pFileTable->pFile[fileindex].inodeNum = pEntry->inodeNum;
            pFileTable->numUsedFile++;

            pFileDescTable->pEntry[freeFD].fileTableIndex = fileindex;
            pFileDescTable->pEntry[freeFD].bUsed = 1;
            pFileDescTable->numUsedDescEntry++;

            return freeFD;
        }
        else if (include && flag == OPEN_FLAG_TRUNCATE)
        {
            int freeFD = invalidFD();
            int fileindex = GetFreeFileTable();
            GetInode(pEntry->inodeNum, pInode);
            removeBlock(pEntry->inodeNum, pInode);

            pFileTable->pFile[fileindex].bUsed = 1;
            pFileTable->pFile[fileindex].fileOffset = 0;
            pFileTable->pFile[fileindex].inodeNum = pEntry->inodeNum;
            pFileTable->numUsedFile++;

            pFileDescTable->pEntry[freeFD].fileTableIndex = fileindex;
            pFileDescTable->pEntry[freeFD].bUsed = 1;
            pFileDescTable->numUsedDescEntry++;

            return freeFD;
        }
    }

    int indirect;
    if ((indirect = GetIndirectBlockEntry(pInode->indirectBlockPtr, 0)) > 0 && i == 4)
    {
        i = 0;
        j = 0;
        while (1)
        {
            for (j = 0; j < 8; j++)
            {
                if (GetDirEntry(indirect, j, pEntry) > 0)
                {
                    if (strcmp(pEntry->name, FileName) == 0)
                    {
                        include = 1;
                        break;
                    }
                }
                else
                    break;
            }
            i++;
            indirect = GetIndirectBlockEntry(pInode->indirectBlockPtr, i);
            if (include && flag == OPEN_FLAG_CREATE)
            {
                int freeFD = invalidFD();
                int fileindex = GetFreeFileTable();

                pFileTable->pFile[fileindex].bUsed = 1;
                pFileTable->pFile[fileindex].fileOffset = 0;
                pFileTable->pFile[fileindex].inodeNum = pEntry->inodeNum;
                pFileTable->numUsedFile++;

                pFileDescTable->pEntry[freeFD].fileTableIndex = fileindex;
                pFileDescTable->pEntry[freeFD].bUsed = 1;
                pFileDescTable->numUsedDescEntry++;

                return freeFD;
            }
            else if (include && flag == OPEN_FLAG_TRUNCATE)
            {
                int freeFD = invalidFD();
                int fileindex = GetFreeFileTable();
                GetInode(pEntry->inodeNum, pInode);
                removeBlock(pEntry->inodeNum, pInode);

                pFileTable->pFile[fileindex].bUsed = 1;
                pFileTable->pFile[fileindex].fileOffset = 0;
                pFileTable->pFile[fileindex].inodeNum = pEntry->inodeNum;
                pFileTable->numUsedFile++;

                pFileDescTable->pEntry[freeFD].fileTableIndex = fileindex;
                pFileDescTable->pEntry[freeFD].bUsed = 1;
                pFileDescTable->numUsedDescEntry++;

                return freeFD;
            }

            if (indirect <= 0)
                break;
        }
    }

    if (flag == OPEN_FLAG_CREATE && include != 1)
    {
        i = 0;
        for (i = 0; i < pInode->allocBlocks; i++)
        {
            j = 0;

            while (j < 8)
            {
                if (GetDirEntry(pInode->dirBlockPtr[i], j, pEntry) < 0)
                {
                    pEntry->inodeNum = freeInode;
                    strcpy(pEntry->name, FileName);
                    PutDirEntry(pInode->dirBlockPtr[i], j, pEntry);
                    break;
                }
                j++;
            }

            if (j < 8)
            {
                break;
            }
        }

        if (i == pInode->allocBlocks && pInode->allocBlocks < 4)
        {
            int new_block = GetFreeBlockNum();
            SetBlockBytemap(new_block);

            pInode->dirBlockPtr[i] = new_block;
            pInode->allocBlocks++;
            pInode->size = 512 * pInode->allocBlocks;
            PutInode(parentInodeNum, pInode);

            strcpy(pEntry->name, FileName);
            pEntry->inodeNum = freeInode;
            PutDirEntry(new_block, 0, pEntry);

            pEntry->inodeNum = INVALID_ENTRY;
            strcpy(pEntry->name, "INVALID_ENTRY");

            for (size_t j = 1; j < 8; j++)
            {
                PutDirEntry(new_block, j, pEntry);
            }

            pBuffer = malloc(BLOCK_SIZE);
            DevReadBlock(FILESYS_INFO_BLOCK, pBuffer);
            pFileSysInfo = (FileSysInfo *)pBuffer;

            pFileSysInfo->numAllocBlocks++;
            pFileSysInfo->numFreeBlocks--;

            DevWriteBlock(FILESYS_INFO_BLOCK, pBuffer);
            free(pBuffer);
        }

        else if (i == pInode->allocBlocks && pInode->allocBlocks >= 4)
        {
            if (pInode->indirectBlockPtr == 0)
            {
                int indirect = GetFreeBlockNum();
                SetBlockBytemap(indirect);

                pInode->indirectBlockPtr = indirect;
                pInode->allocBlocks++;
                pInode->size = 512 * pInode->allocBlocks;
                PutInode(parentInodeNum, pInode);

                int next = GetFreeBlockNum();
                PutIndirectBlockEntry(indirect, 0, next);

                SetBlockBytemap(next);

                for (size_t j = 1; j < BLOCK_SIZE / sizeof(int); j++)
                {
                    PutIndirectBlockEntry(indirect, j, 0);
                }

                strcpy(pEntry->name, FileName);
                pEntry->inodeNum = freeInode;
                PutDirEntry(GetIndirectBlockEntry(indirect, 0), 0, pEntry);

                pEntry->inodeNum = INVALID_ENTRY;
                strcpy(pEntry->name, "INVALID_ENTRY");

                for (size_t j = 1; j < 8; j++)
                {
                    PutDirEntry(GetIndirectBlockEntry(indirect, 0), j, pEntry);
                }

                pBuffer = malloc(BLOCK_SIZE);
                DevReadBlock(FILESYS_INFO_BLOCK, pBuffer);
                pFileSysInfo = (FileSysInfo *)pBuffer;

                pFileSysInfo->numAllocBlocks += 2;
                pFileSysInfo->numFreeBlocks -= 2;

                DevWriteBlock(FILESYS_INFO_BLOCK, pBuffer);
                free(pBuffer);
            }

            else
            {
                int ptr;
                int j;
                for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
                {
                    ptr = GetIndirectBlockEntry(pInode->indirectBlockPtr, j);
                    if (ptr == 0)
                        break;
                }

                ptr = GetIndirectBlockEntry(pInode->indirectBlockPtr, j - 1);
                int k = 0;
                for (k = 0; k < 8; k++)
                {
                    if (GetDirEntry(ptr, k, pEntry) < 0)
                    {
                        strcpy(pEntry->name, FileName);
                        pEntry->inodeNum = freeInode;
                        PutDirEntry(ptr, k, pEntry);
                        break;
                    }
                }

                if (k == 8)
                {
                    int block = GetFreeBlockNum();
                    SetBlockBytemap(block);
                    PutIndirectBlockEntry(pInode->indirectBlockPtr, j, block);

                    strcpy(pEntry->name, FileName);
                    pEntry->inodeNum = freeInode;
                    int index = GetIndirectBlockEntry(pInode->indirectBlockPtr, j);

                    PutDirEntry(index, 0, pEntry);

                    pEntry->inodeNum = INVALID_ENTRY;
                    strcpy(pEntry->name, "INVALID_ENTRY");

                    for (j = 1; j < 8; j++)
                    {
                        PutDirEntry(index, j, pEntry);
                    }
                }
            }
        }
        GetInode(freeInode, pInode);

        pInode->allocBlocks = 0;
        pInode->dirBlockPtr[0] = 0;
        pInode->indirectBlockPtr = 0;
        pInode->size = 0;
        pInode->type = FILE_TYPE_FILE;

        PutInode(freeInode, pInode);

        pBuffer = malloc(BLOCK_SIZE);
        DevReadBlock(FILESYS_INFO_BLOCK, pBuffer);
        pFileSysInfo = (FileSysInfo *)pBuffer;

        pFileSysInfo->numAllocInodes++;

        DevWriteBlock(FILESYS_INFO_BLOCK, pBuffer);
        free(pBuffer);

        int freeFD = invalidFD();
        int freeFiletable = GetFreeFileTable();

        pFileTable->pFile[freeFiletable].bUsed = 1;
        pFileTable->pFile[freeFiletable].fileOffset = 0;
        pFileTable->pFile[freeFiletable].inodeNum = freeInode;
        pFileTable->numUsedFile++;

        pFileDescTable->pEntry[freeFD].fileTableIndex = freeFiletable;
        pFileDescTable->pEntry[freeFD].bUsed = 1;
        pFileDescTable->numUsedDescEntry++;

        return freeFD;
    }
}

int WriteFile(int fileDesc, char *pBuffer, int length)
{
    int file_index;
    int logical_block;
    
    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    char *Buffer = malloc(BLOCK_SIZE);

    DevReadBlock(FILESYS_INFO_BLOCK, Buffer);
    pFileSysInfo = (FileSysInfo *)Buffer;
    
    for (size_t i = 0; i < length / BLOCK_SIZE; i++)
    {
        int free_block = GetFreeBlockNum();
        SetBlockBytemap(free_block);
        
        pFileSysInfo->numAllocBlocks++;
        pFileSysInfo->numFreeBlocks--;

        file_index = pFileDescTable->pEntry[fileDesc].fileTableIndex;
        logical_block = pFileTable->pFile[file_index].fileOffset / BLOCK_SIZE;
        GetInode(pFileTable->pFile[file_index].inodeNum, pInode);

        if (logical_block < 4)
        {
            pInode->dirBlockPtr[logical_block] = free_block;
            pInode->allocBlocks++;
            pInode->size = pInode->allocBlocks * BLOCK_SIZE;
        }
        else
        {

            if (pInode->indirectBlockPtr < 1)
            {
                int free_block2 = GetFreeBlockNum();
                SetBlockBytemap(free_block2);

                pInode->indirectBlockPtr = free_block2;

                PutIndirectBlockEntry(free_block2, 0, free_block);

                pInode->allocBlocks++;
                pInode->size = 512 * pInode->allocBlocks;

                pFileSysInfo->numAllocBlocks++;
                pFileSysInfo->numFreeBlocks--;
            }
            else
            {
                for (size_t i = 1; i < BLOCK_SIZE / sizeof(int); i++)
                {
                    if (GetIndirectBlockEntry(pInode->indirectBlockPtr, i) < 1)
                    {
                        PutIndirectBlockEntry(pInode->indirectBlockPtr, i, free_block);
                        pInode->allocBlocks++;
                        pInode->size = 512 * pInode->allocBlocks;
                        break;
                    }
                }
            }
        }
    
        pFileTable->pFile[file_index].fileOffset += BLOCK_SIZE;

        Buffer = malloc(BLOCK_SIZE);
        memcpy(Buffer, pBuffer + (i * BLOCK_SIZE), BLOCK_SIZE);
        DevWriteBlock(free_block, Buffer);

        PutInode(pFileTable->pFile[file_index].inodeNum, pInode);

        DevWriteBlock(FILESYS_INFO_BLOCK, (char *)pFileSysInfo);
    }

    free(pFileSysInfo);
    free(pInode);
    
    return length;
}

int ReadFile(int fileDesc, char *pBuffer, int length)
{
    int file_index, logical_block;
    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    char* temp = malloc(BLOCK_SIZE);

    for (size_t i = 0; i < length / BLOCK_SIZE; i++)
    {
        file_index = pFileDescTable->pEntry[fileDesc].fileTableIndex;
        logical_block = pFileTable->pFile[file_index].fileOffset / BLOCK_SIZE;
        GetInode(pFileTable->pFile[file_index].inodeNum, pInode);

        if (logical_block < 4)
        {
            DevReadBlock(pInode->dirBlockPtr[logical_block], temp);
            memcpy(pBuffer + (i * BLOCK_SIZE), temp, BLOCK_SIZE);
        }
        else
        {
            if (pInode->indirectBlockPtr > 0)
            {
                int blk = GetIndirectBlockEntry(pInode->indirectBlockPtr, logical_block - 4);
                DevReadBlock(blk, temp);
                memcpy(pBuffer + (i * BLOCK_SIZE), temp, BLOCK_SIZE);
            }
        }
        pFileTable->pFile[file_index].fileOffset += BLOCK_SIZE;
    }

    return length;
}

int CloseFile(int fileDesc)
{
    if (!pFileDescTable->pEntry[fileDesc].bUsed)
        return -1;

    pFileDescTable->pEntry[fileDesc].fileTableIndex = -1;
    pFileDescTable->pEntry[fileDesc].bUsed = 0;
    pFileDescTable->numUsedDescEntry--;

    return 1;
}

int RemoveFile(char *name)
{
    int file_inode = searchDir(name);
    int parentInodeNum;
    Inode* pInode = (Inode*)malloc(sizeof(Inode)); 
    DirEntry* pEntry;
    GetInode(file_inode, pInode);

    char* pBuffer;
 
    char* split;
    char path[MAX_NAME_LEN];
    char FileName[MAX_NAME_LEN];

    if (pInode->type != FILE_TYPE_FILE)
    {
        return -1;
    }
    
    strcpy(path, name);
    split = strtok(path, "/");

    while (split != NULL)
    {
        strcpy(FileName, split);
        split = strtok(NULL, "/");
    }

    int searchPathLen = strlen(name) - strlen(FileName) - 1;

    if (searchPathLen == 0)
    {
        parentInodeNum = 0;
    }

    else
    {
        char searchPath[searchPathLen];

        int i;
        for (i = 0; i < searchPathLen; i++)
        {
            searchPath[i] = name[i];
        }

        searchPath[i] = '\0';

        parentInodeNum = searchDir(searchPath);
        if (parentInodeNum < 0)
        {
            return -1;
        }
    }
    removeBlock(file_inode, pInode);
    GetInode(parentInodeNum, pInode);
    pEntry = (DirEntry*)malloc(sizeof(DirEntry));

    pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(FILESYS_INFO_BLOCK, pBuffer);
    pFileSysInfo = (FileSysInfo*)pBuffer;

    pFileSysInfo->numAllocInodes--;
    DevWriteBlock(FILESYS_INFO_BLOCK, (char*)pFileSysInfo);

    for (size_t i = 0; i < 4; i++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            GetDirEntry(pInode->dirBlockPtr[i], j, pEntry);
            if (pEntry->inodeNum == file_inode)
            {
                int k = j;
                int m = i;
                while(m < 4){
                    if(k < 7){
                        GetDirEntry(pInode->dirBlockPtr[m], k + 1, pEntry);
                        PutDirEntry(pInode->dirBlockPtr[m], k, pEntry);

                        if(pEntry->inodeNum < 0) {
                            
                            free(pInode);
                            free(pEntry);
                            free(pFileSysInfo);
                            
                            return 1;
                        }
                        k++;
                    }
                    else{
                        m++;
                        GetDirEntry(pInode->dirBlockPtr[m], 0, pEntry);
                        PutDirEntry(pInode->dirBlockPtr[m - 1], 7, pEntry);
                        k = 0;

                        if(pEntry->inodeNum < 0){
                            pInode->allocBlocks--;
                            pInode->dirBlockPtr[m] = 0;
                            pInode->size = pInode->allocBlocks * 512;

                            pFileSysInfo->numAllocBlocks--;
                            pFileSysInfo->numFreeBlocks++;
                            DevWriteBlock(FILESYS_INFO_BLOCK, (char*)pFileSysInfo);
                            
                            PutInode(parentInodeNum, pInode);
                            
                            free(pInode);
                            free(pEntry);
                            free(pFileSysInfo);
                            return 1;
                        } 
                    }
                }
            }
        }
    }
}

int MakeDirectory(char *name)
{
    int parentInodeNum;
    int freeInodeNum = GetFreeInodeNum();
    int freeBlockNum = GetFreeBlockNum();

    SetBlockBytemap(freeBlockNum);
    SetInodeBytemap(freeInodeNum);

    char path[strlen(name)];
    char newDirName[MAX_NAME_LEN];
    char *split;
    char *pBuffer;
    
    pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(FILESYS_INFO_BLOCK, pBuffer);
    pFileSysInfo = (FileSysInfo *)pBuffer;
    
    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    DirEntry *pEntry = (DirEntry *)malloc(sizeof(DirEntry));

    strcpy(path, name);
    split = strtok(path, "/");

    while (split != NULL)
    {
        strcpy(newDirName, split);
        split = strtok(NULL, "/");
    }

    int searchPathLen = strlen(name) - strlen(newDirName) - 1;

    if (searchPathLen == 0)
    {
        parentInodeNum = 0;
    }

    else
    {
        char searchPath[searchPathLen];

        int i;
        for (i = 0; i < searchPathLen; i++)
        {
            searchPath[i] = name[i];
        }

        searchPath[i] = '\0';
        parentInodeNum = searchDir(searchPath);
        if (parentInodeNum < 0)
        {
            return -1;
        }
    }

    GetInode(parentInodeNum, pInode);

    int i = 0;
    for (i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++)
    {
        int j = 0;

        while (j < 8)
        {
            if (GetDirEntry(pInode->dirBlockPtr[i], j, pEntry) == -1)
            {
                pEntry->inodeNum = freeInodeNum;
                strcpy(pEntry->name, newDirName);
                PutDirEntry(pInode->dirBlockPtr[i], j, pEntry);
                break;
            }
            j++;
        }

        if (j < 8)
        {
            break;
        }
    }

    if (i == pInode->allocBlocks && pInode->allocBlocks < 4)
    {
        int new_block = GetFreeBlockNum();
        SetBlockBytemap(new_block);

        pFileSysInfo->numAllocBlocks++;
        pFileSysInfo->numFreeBlocks--;

        pInode->dirBlockPtr[i] = new_block;
        pInode->allocBlocks++;
        pInode->size = 512 * pInode->allocBlocks;
        PutInode(parentInodeNum, pInode);

        strcpy(pEntry->name, newDirName);
        pEntry->inodeNum = freeInodeNum;
        PutDirEntry(new_block, 0, pEntry);

        pEntry->inodeNum = INVALID_ENTRY;
        strcpy(pEntry->name, "INVALID_ENTRY");

        for (size_t j = 1; j < 8; j++)
        {
            PutDirEntry(new_block, j, pEntry);
        }

        DevWriteBlock(FILESYS_INFO_BLOCK, (char*)pFileSysInfo);
    }

    else if (i == NUM_OF_DIRECT_BLOCK_PTR && pInode->allocBlocks >= 4)
    {
        if (pInode->indirectBlockPtr == 0)
        {
            int indirect = GetFreeBlockNum();
            SetBlockBytemap(indirect);

            pFileSysInfo->numAllocBlocks++;
            pFileSysInfo->numFreeBlocks--;

            pInode->indirectBlockPtr = indirect;
            pInode->allocBlocks++;
            pInode->size = pInode->allocBlocks * 512;
            PutInode(parentInodeNum, pInode);

            int next = GetFreeBlockNum();
            PutIndirectBlockEntry(indirect, 0, next);

            pFileSysInfo->numAllocBlocks++;
            pFileSysInfo->numFreeBlocks--;

            SetBlockBytemap(next);

            for (size_t j = 1; j < BLOCK_SIZE / sizeof(int); j++)
            {
                PutIndirectBlockEntry(indirect, j, 0);
            }

            strcpy(pEntry->name, newDirName);
            pEntry->inodeNum = freeInodeNum;
            PutDirEntry(GetIndirectBlockEntry(indirect, 0), 0, pEntry);

            pEntry->inodeNum = INVALID_ENTRY;
            strcpy(pEntry->name, "INVALID_ENTRY");

            for (size_t j = 1; j < 8; j++)
            {
                PutDirEntry(GetIndirectBlockEntry(indirect, 0), j, pEntry);
            }

            DevWriteBlock(FILESYS_INFO_BLOCK, (char*)pFileSysInfo);
        }

        else
        {
            int ptr;
            int j;
            for (j = 0; j < BLOCK_SIZE / sizeof(int); j++)
            {
                ptr = GetIndirectBlockEntry(pInode->indirectBlockPtr, j);
                if (ptr == 0)
                    break;
            }

            ptr = GetIndirectBlockEntry(pInode->indirectBlockPtr, j - 1);
            int k = 0;
            for (k = 0; k < 8; k++)
            {
                if (GetDirEntry(ptr, k, pEntry) < 0)
                {
                    strcpy(pEntry->name, newDirName);
                    pEntry->inodeNum = freeInodeNum;
                    PutDirEntry(ptr, k, pEntry);
                    break;
                }
            }

            if (k == 8)
            {
                int block = GetFreeBlockNum();
                SetBlockBytemap(block);
                
                pFileSysInfo->numAllocBlocks++;
                pFileSysInfo->numFreeBlocks--;
                DevWriteBlock(FILESYS_INFO_BLOCK, (char*)pFileSysInfo);

                PutIndirectBlockEntry(pInode->indirectBlockPtr, j, block);

                pInode->allocBlocks++;
                pInode->size = pInode->allocBlocks * 512;
                PutInode(parentInodeNum, pInode);

                strcpy(pEntry->name, newDirName);
                pEntry->inodeNum = freeInodeNum;
                int index = GetIndirectBlockEntry(pInode->indirectBlockPtr, j);
                PutDirEntry(index, 0, pEntry);

                pEntry->inodeNum = INVALID_ENTRY;
                strcpy(pEntry->name, "INVALID_ENTRY");

                for (j = 1; j < 8; j++)
                {
                    PutDirEntry(index, j, pEntry);
                }
            }
        }
    }

    free(pEntry);

    pBuffer = malloc(BLOCK_SIZE);

    DevReadBlock(freeBlockNum, pBuffer);
    pEntry = (DirEntry *)pBuffer;

    strcpy(pEntry[0].name, ".");
    pEntry[0].inodeNum = freeInodeNum;

    strcpy(pEntry[1].name, "..");
    pEntry[1].inodeNum = parentInodeNum;

    for (i = 2; i < 8; i++)
    {
        pEntry[i].inodeNum = INVALID_ENTRY;
        strcpy(pEntry[i].name, "INVALID_ENTRY");
    }

    DevWriteBlock(freeBlockNum, (char *)pEntry);
    free(pBuffer);

    GetInode(freeInodeNum, pInode);

    pInode->dirBlockPtr[0] = freeBlockNum;
    pInode->allocBlocks = 1;
    pInode->type = FILE_TYPE_DIR;
    pInode->size = pInode->allocBlocks * 512;
    pInode->indirectBlockPtr = 0;

    PutInode(freeInodeNum, pInode);
    free(pInode);

    pFileSysInfo->numAllocInodes++;
    pFileSysInfo->numAllocBlocks++;
    pFileSysInfo->numFreeBlocks--;
    
    DevWriteBlock(FILESYS_INFO_BLOCK, (char *)pFileSysInfo);
    free(pFileSysInfo);

    return 0;
}

int RemoveDirectory(char *name)
{
    int file_inode = searchDir(name);
    int parentInodeNum;

    Inode* pInode = (Inode*)malloc(sizeof(Inode)); 
    DirEntry* pEntry = (DirEntry*)malloc(sizeof(DirEntry));
    GetInode(file_inode, pInode);
    char* pBuffer;

    pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(FILESYS_INFO_BLOCK, pBuffer);
    pFileSysInfo = (FileSysInfo *)pBuffer;

    char *split;
    char path[MAX_NAME_LEN];
    char FileName[MAX_NAME_LEN];


    if(GetDirEntry(pInode->dirBlockPtr[0], 2, pEntry) > 0) return -1;

    if (pInode->type != FILE_TYPE_DIR)
    {
        return -1;
    }

    strcpy(path, name);
    split = strtok(path, "/");

    while (split != NULL)
    {
        strcpy(FileName, split);
        split = strtok(NULL, "/");
    }

    int searchPathLen = strlen(name) - strlen(FileName) - 1;

    if (searchPathLen == 0)
    {
        parentInodeNum = 0;
    }

    else
    {
        char searchPath[searchPathLen];

        int i;
        for (i = 0; i < searchPathLen; i++)
        {
            searchPath[i] = name[i];
        }

        searchPath[i] = '\0';

        parentInodeNum = searchDir(searchPath);
        if (parentInodeNum < 0)
        {
            return -1;
        }
    }
    GetInode(parentInodeNum, pInode);

    for (size_t i = 0; i < 4; i++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            if(GetDirEntry(pInode->dirBlockPtr[i], j, pEntry) < 0) return -1;
            if(pEntry->inodeNum == file_inode){
                RemoveDirEntry(pInode->dirBlockPtr[i], j);
                ResetInodeBytemap(file_inode);

                if(j == 0){
                    ResetBlockBytemap(pInode->dirBlockPtr[i]);
                    
                    pInode->dirBlockPtr[i] = 0;
                    pInode->allocBlocks--;
                    pInode->size = pInode->allocBlocks * 512;
                    PutInode(parentInodeNum, pInode);

                    pFileSysInfo->numAllocBlocks--;
                    pFileSysInfo->numFreeBlocks++;
                }

                pFileSysInfo->numAllocBlocks--;
                pFileSysInfo->numFreeBlocks++;
                pFileSysInfo->numAllocInodes--;
                
                DevWriteBlock(FILESYS_INFO_BLOCK, (char *)pFileSysInfo);
                free(pBuffer);
                return 1;
            }   
        }
    }

    int indirect;
    if((indirect = GetIndirectBlockEntry(pInode->indirectBlockPtr, 0)) > 0){
        int j = 0;
        while (1)
        {
            for (size_t i = 0; i < 8; i++)
            {
                if(GetDirEntry(indirect, i, pEntry) < 0) return -1;
                if(pEntry->inodeNum == file_inode){
                    RemoveDirEntry(indirect, i);
                    ResetInodeBytemap(file_inode);

                    if(i == 0){
                        ResetBlockBytemap(indirect);
                        RemoveIndirectBlockEntry(indirect, j);

                        pInode->allocBlocks--;
                        pInode->size = pInode->allocBlocks * 512;
                        PutInode(parentInodeNum, pInode);
                        
                        pFileSysInfo->numAllocBlocks--;
                        pFileSysInfo->numFreeBlocks++;

                        if(j == 0){
                            pFileSysInfo->numAllocBlocks--;
                            pFileSysInfo->numFreeBlocks++;
                        }
                    }
                    
                    pFileSysInfo->numAllocBlocks--;
                    pFileSysInfo->numFreeBlocks++;
                    pFileSysInfo->numAllocInodes--;

                    DevWriteBlock(FILESYS_INFO_BLOCK, (char *)pFileSysInfo);
                    free(pBuffer);
                    return 1;
                }
            }
            j++;
            indirect = GetIndirectBlockEntry(pInode->indirectBlockPtr, j);
        }
    }
}

void CreateFileSystem(void)
{
    FileSysInit();
    int free_block = GetFreeBlockNum();
    int root_inode = GetFreeInodeNum();

    Inode *pInode = (Inode *)malloc(sizeof(Inode));

    pFileDescTable = (FileDescTable *)malloc(sizeof(FileDescTable));
    pFileDescTable->numUsedDescEntry = 0;
    for (size_t i = 0; i < DESC_ENTRY_NUM; i++)
    {
        pFileDescTable->pEntry[i].fileTableIndex = -1;
        pFileDescTable->pEntry[i].bUsed = 0;
    }

    pFileTable = (FileTable *)malloc(sizeof(FileTable));
    for (size_t i = 0; i < MAX_FILE_NUM; i++)
    {
        pFileTable->pFile[i].bUsed = 0;
    }

    // create root directory
    char *pBuffer = malloc(BLOCK_SIZE);
    DirEntry *temp1 = (DirEntry *)pBuffer;

    strcpy(temp1[0].name, ".");
    temp1[0].inodeNum = root_inode;

    for (size_t i = 1; i < 8; i++)
    {
        temp1[i].inodeNum = INVALID_ENTRY;
        strcpy(temp1[i].name, "INVALID_ENTRY");
    }

    DevWriteBlock(free_block, (char *)temp1);
    SetBlockBytemap(free_block);
    free(pBuffer);

    // Initailize FilesysInfo
    pBuffer = malloc(BLOCK_SIZE);
    pFileSysInfo = (FileSysInfo *)pBuffer;

    pFileSysInfo->blocks = 512;
    pFileSysInfo->rootInodeNum = root_inode;
    pFileSysInfo->diskCapacity = FS_DISK_CAPACITY;
    pFileSysInfo->numAllocBlocks = 0;
    pFileSysInfo->numFreeBlocks = 512 - 11;
    pFileSysInfo->numAllocInodes = 0;
    pFileSysInfo->blockBytemapBlock = BLOCK_BYTEMAP_BLOCK_NUM;
    pFileSysInfo->inodeBytemapBlock = INODE_BYTEMAP_BLOCK_NUM;
    pFileSysInfo->inodeListBlock = INODELIST_BLOCK_FIRST;
    pFileSysInfo->dataRegionBlock = 11;

    pFileSysInfo->numAllocBlocks++;
    pFileSysInfo->numFreeBlocks--;
    pFileSysInfo->numAllocInodes++;

    DevWriteBlock(FILESYS_INFO_BLOCK, (char *)pFileSysInfo);
    free(pBuffer);

    // Set Inode
    GetInode(0, pInode);

    pInode->dirBlockPtr[0] = 11;
    pInode->allocBlocks = 1;
    pInode->type = FILE_TYPE_DIR;
    pInode->size = pInode->allocBlocks * 512;
    pInode->indirectBlockPtr = -1;

    PutInode(0, pInode);
    SetInodeBytemap(root_inode);
    free(pInode);
}

void OpenFileSystem(void)
{
    pFileDescTable = (FileDescTable *)malloc(sizeof(FileDescTable));
    pFileDescTable->numUsedDescEntry = 0;
    for (size_t i = 0; i < DESC_ENTRY_NUM; i++)
    {
        pFileDescTable->pEntry[i].fileTableIndex = -1;
        pFileDescTable->pEntry[i].bUsed = 0;
    }

    pFileTable = (FileTable *)malloc(sizeof(FileTable));
    for (size_t i = 0; i < MAX_FILE_NUM; i++)
    {
        pFileTable->pFile[i].bUsed = 0;
    }

    DevOpenDisk();
}

void CloseFileSystem(void)
{
    DevCloseDisk();
}

Directory *OpenDirectory(char *name)
{
    Directory *result = (Directory *)malloc(sizeof(Directory));

    if (strcmp(name, "/") == 0)
    {
        result->inodeNum = 0;
    }
    else
    {
        result->inodeNum = searchDir(name);
    }

    if (result->inodeNum < 0)
    {
        return NULL;
    }

    return result;
}

int previousInodeNum = -1;
int accessTime = 0;

FileInfo *ReadDirectory(Directory *pDir)
{
    FileInfo *result = (FileInfo *)malloc(sizeof(FileInfo));
    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    DirEntry *temp = (DirEntry *)malloc(sizeof(DirEntry));

    int blockIndex, entryIndex;
    GetInode(pDir->inodeNum, pInode);

    if (previousInodeNum != pDir->inodeNum)
    {
        accessTime = 0;
        previousInodeNum = pDir->inodeNum;
    }

    blockIndex = accessTime / 8;
    entryIndex = accessTime % 8;

    if (accessTime > 31)
    {
        int ptr = GetIndirectBlockEntry(pInode->indirectBlockPtr, blockIndex - 4);
        if (GetDirEntry(ptr, entryIndex, temp) < 0)
        {
            accessTime = 0;
            previousInodeNum = -1;
            free(pInode);
            free(temp);
            return NULL;
        }

        GetInode(temp->inodeNum, pInode);

        result->filetype = pInode->type;
        result->inodeNum = temp->inodeNum;
        strcpy(result->name, temp->name);
        result->numBlocks = pInode->allocBlocks;
        result->size = pInode->size;

        accessTime++;
        free(pInode);
        free(temp);

        return result;
    }

    if (GetDirEntry(pInode->dirBlockPtr[blockIndex], entryIndex, temp) < 0)
    {
        accessTime = 0;
        previousInodeNum = -1;
        free(pInode);
        free(temp);
        return NULL;
    }

    GetInode(temp->inodeNum, pInode);

    result->filetype = pInode->type;
    result->inodeNum = temp->inodeNum;
    strcpy(result->name, temp->name);
    result->numBlocks = pInode->allocBlocks;
    result->size = pInode->size;

    accessTime++;
    free(pInode);
    free(temp);

    return result;
}

int CloseDirectory(Directory *pDir)
{
    if (pDir == NULL)
    {
        return -1;
    }

    free(pDir);
    return 0;
}

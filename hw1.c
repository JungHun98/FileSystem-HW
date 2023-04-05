#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "hw1.h"

void FileSysInit()
{   
    DevCreateDisk(); //open Disk

    char* block_init = malloc(BLOCK_SIZE);
    for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
        block_init[i] = (char)0;
    }
	
    for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
        DevWriteBlock(i, block_init);
    }

    free(block_init);
}

void SetInodeBytemap(int inodeno)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(INODE_BYTEMAP_BLOCK_NUM, pBuffer);

    pBuffer[inodeno] = (char)1;
    DevWriteBlock(INODE_BYTEMAP_BLOCK_NUM, pBuffer);

    free(pBuffer);
}

void ResetInodeBytemap(int inodeno)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(INODE_BYTEMAP_BLOCK_NUM, pBuffer);

    pBuffer[inodeno] = (char)0;
    DevWriteBlock(INODE_BYTEMAP_BLOCK_NUM, pBuffer);

    free(pBuffer);
}

void SetBlockBytemap(int blkno)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuffer);

    pBuffer[blkno] = (char)1;
    DevWriteBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuffer);

    free(pBuffer);
}

void ResetBlockBytemap(int blkno)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuffer);

    pBuffer[blkno] = (char)0;
    DevWriteBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuffer);

    free(pBuffer);
}

void PutInode(int inodeno, Inode* pInode)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    int reinodeno = inodeno % 16;
    int blkno = (inodeno / 16) + 3;

    DevReadBlock(blkno, pBuffer);
    Inode* temp = (Inode*)pBuffer;

    temp[reinodeno].size = pInode->size;
    temp[reinodeno].type = pInode->type;
    temp[reinodeno].allocBlocks = pInode->allocBlocks;
    temp[reinodeno].indirectBlockPtr = pInode->indirectBlockPtr;

    for (size_t i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++)
    {
        temp[reinodeno].dirBlockPtr[i] = pInode->dirBlockPtr[i];
    }

    DevWriteBlock(blkno, (char*)temp);
    free(pBuffer);
}

void GetInode(int inodeno, Inode* pInode)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    int reinodeno = inodeno % 16; //Inode index in Block
    int blkno = (inodeno / 16) + 3; //Block number that has Inode information of inodeno

    DevReadBlock(blkno, pBuffer);
    Inode* temp = (Inode*)pBuffer;

    pInode->size = temp[reinodeno].size;
    pInode->type = temp[reinodeno].type;
    pInode->allocBlocks = temp[reinodeno].allocBlocks;
    pInode->indirectBlockPtr = temp[reinodeno].indirectBlockPtr;

    for (size_t i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++)
    {
        pInode->dirBlockPtr[i] = temp[reinodeno].dirBlockPtr[i];
    }

    free(pBuffer);
}

int GetFreeInodeNum(void)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    
    DevReadBlock(1, pBuffer);
    for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
        if (pBuffer[i] == (char)0)
        {
            free(pBuffer);
            return i;
        }
    }
    free(pBuffer);
    return -1;
}

int GetFreeBlockNum(void)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(2, pBuffer);
    for (size_t i = 11; i < BLOCK_SIZE; i++)
    {
        if (pBuffer[i] == (char)0)
        {
            free(pBuffer);
            return i;
        }
    }

    free(pBuffer);
    return -1;
}

void PutIndirectBlockEntry(int blkno, int index, int number)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, pBuffer);
    int* temp = (int*)pBuffer;

    temp[index] = number;

    DevWriteBlock(blkno, (char*)temp);
    free(pBuffer);
}

int GetIndirectBlockEntry(int blkno, int index)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    int result;

    DevReadBlock(blkno, pBuffer);
    int* temp = (int*)pBuffer;

    result = temp[index];
    free(pBuffer);

    return result;
}

void RemoveIndirectBlockEntry(int blkno, int index){
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, pBuffer);
    int* temp = (int*)pBuffer;

    temp[index] = INVALID_ENTRY;

    DevWriteBlock(blkno, (char*)temp);
    free(pBuffer);
}

void PutDirEntry(int blkno, int index, DirEntry* pEntry)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, pBuffer);
    DirEntry* temp = (DirEntry*)pBuffer;

    temp[index].inodeNum = pEntry->inodeNum;
    strcpy(temp[index].name, pEntry->name);
    
    DevWriteBlock(blkno, (char*)temp);

    free(pBuffer);
}

int GetDirEntry(int blkno, int index, DirEntry* pEntry)
{
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, pBuffer);
    DirEntry* temp = (DirEntry*)pBuffer;

    pEntry->inodeNum = temp[index].inodeNum;
    strcpy(pEntry->name, temp[index].name);

    if(temp[index].inodeNum == INVALID_ENTRY){
        return -1;
    }

    free(pBuffer);

    return 1;
}

void RemoveDirEntry(int blkno, int index){
    char* pBuffer = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, pBuffer);
    DirEntry* temp = (DirEntry*)pBuffer;

    temp[index].inodeNum = INVALID_ENTRY;
    
    DevWriteBlock(blkno, (char*)temp);
    free(pBuffer);
}

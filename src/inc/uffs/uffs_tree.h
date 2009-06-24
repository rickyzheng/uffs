/*
  This file is part of UFFS, the Ultra-low-cost Flash File System.
  
  Copyright (C) 2005-2009 Ricky Zheng <ricky_gz_zheng@yahoo.co.nz>

  UFFS is free software; you can redistribute it and/or modify it under
  the GNU Library General Public License as published by the Free Software 
  Foundation; either version 2 of the License, or (at your option) any
  later version.

  UFFS is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  or GNU Library General Public License, as applicable, for more details.
 
  You should have received a copy of the GNU General Public License
  and GNU Library General Public License along with UFFS; if not, write
  to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301, USA.

  As a special exception, if other files instantiate templates or use
  macros or inline functions from this file, or you compile this file
  and link it with other works to produce a work based on this file,
  this file does not by itself cause the resulting work to be covered
  by the GNU General Public License. However the source code for this
  file must still be made available in accordance with section (3) of
  the GNU General Public License v2.
 
  This exception does not invalidate any other reasons why a work based
  on this file might be covered by the GNU General Public License.
*/

#ifndef _UFFS_TREE_H_
#define _UFFS_TREE_H_

#include "uffs/uffs_types.h"
#include "uffs/ubuffer.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_core.h"

#ifdef __cplusplus
extern "C"{
#endif


#define UFFS_TYPE_DIR		0
#define UFFS_TYPE_FILE		1
#define UFFS_TYPE_DATA		2
#define UFFS_TYPE_RESV		3
#define UFFS_TYPE_INVALID	0xFF


#define SERIAL_ROOT_DIR		0

struct blocklistSt {	/* 10 bytes */
	struct uffs_treeNodeSt * next;
	struct uffs_treeNodeSt * prev;
	u16 block;
};

struct dirhSt {		/* 8 bytes */
	u16 checkSum;	/* check sum of dir name */
	u16 block;
	u16 father;
	u16 serial;
};


struct filehSt {	/* 12 bytes */
	u16 block;
	u16 checkSum;	/* check sum of file name */
	u16 father;
	u16 serial;
	u32 len;		/* file length total */
};

struct fdataSt {	/* 10 bytes */
	u16 block;
	u16 father;
	u32 len;		/* file data length on this block */
	u16 serial;
};

//UFFS TreeNode (14 or 16 bytes)
typedef struct uffs_treeNodeSt {
	union {
		struct blocklistSt list;
		struct dirhSt dir;
		struct filehSt file;
		struct fdataSt data;
	} u;
	u16 hashNext;		
#ifdef TREE_NODE_USE_DOUBLE_LINK
	u16 hashPrev;			
#endif
} TreeNode;


//TODO: UFFS2 Tree structures
/*
struct fdataSt {
	u32 len;
};

struct filebSt {
	u16 bls;		//how many blocks this file contents ...
	u8 offs;		//the offset of this file header on FILE block
	u8 sum;			//short sum of file name
};

//Extra data structure for storing file length information
struct filehSt {
	u32 len;
};

//UFFS2 TreeNode (12 bytes)
typedef struct uffs_treeNodeSt {
	u16 nextIdx;
	u16 block;
	u16 father;
	u16 serial;
	union {
		struct filehSt h;
		struct filedSt file;
		struct data;
	} u;
} TreeNode;

*/


#define EMPTY_NODE 0xffff		//!< special index num of empty node.

#define ROOT_DIR_ID	0			//!< serial num of root dir


#define MAX_UFFS_SERIAL			0xfffe
#define PARENT_OF_ROOT			0xfffd
#define INVALID_UFFS_SERIAL		0xffff

#define DIR_NODE_HASH_MASK		0x1f
#define DIR_NODE_ENTRY_LEN		(DIR_NODE_HASH_MASK + 1)

#define FILE_NODE_HASH_MASK		0x3f
#define FILE_NODE_ENTRY_LEN		(FILE_NODE_HASH_MASK + 1)

#define DATA_NODE_HASH_MASK		0x1ff
#define DATA_NODE_ENTRY_LEN		(DATA_NODE_HASH_MASK + 1)
#define FROM_IDX(idx, dis)		((TreeNode *)uBufGetBufByIndex(idx, dis))
#define TO_IDX(p, dis)			((u16)uBufGetIndex((void *)p, dis))


#define GET_FILE_HASH(serial)			(serial & FILE_NODE_HASH_MASK)
#define GET_DIR_HASH(serial)			(serial & DIR_NODE_HASH_MASK)
#define GET_DATA_HASH(father, serial)	((father + serial) & DATA_NODE_HASH_MASK)


struct uffs_treeSt {
	TreeNode *erased;					//!< erased block list head
	TreeNode *erased_tail;				//!< erased block list tail
	int erasedCount;					//!< erased block counter
	TreeNode *bad;						//!< bad block list
	int badCount;						//!< bad block count
	u16 dirEntry[DIR_NODE_ENTRY_LEN];
	u16 fileEntry[FILE_NODE_ENTRY_LEN];
	u16 dataEntry[DATA_NODE_ENTRY_LEN];
	struct ubufm dis;
	u16 maxSerialNo;
};


URET uffs_InitTreeBuf(uffs_Device *dev);
URET uffs_ReleaseTreeBuf(uffs_Device *dev);
URET uffs_BuildTree(uffs_Device *dev);
u16 uffs_FindFreeFsnSerial(uffs_Device *dev);
TreeNode * uffs_FindFileNodeFromTree(uffs_Device *dev, u16 serial);
TreeNode * uffs_FindFileNodeFromTreeWithFather(uffs_Device *dev, u16 father);
TreeNode * uffs_FindDirNodeFromTree(uffs_Device *dev, u16 serial);
TreeNode * uffs_FindDirNodeFromTreeWithFather(uffs_Device *dev, u16 father);
TreeNode * uffs_FindFileNodeByName(uffs_Device *dev, const char *name, u32 len, u16 sum, u16 father);
TreeNode * uffs_FindDirNodeByName(uffs_Device *dev, const char *name, u32 len, u16 sum, u16 father);
TreeNode * uffs_FindDataNode(uffs_Device *dev, u16 father, u16 serial);


TreeNode * uffs_FindDirNodeByBlock(uffs_Device *dev, u16 block);
TreeNode * uffs_FindFileNodeByBlock(uffs_Device *dev, u16 block);
TreeNode * uffs_FindDataNodeByBlock(uffs_Device *dev, u16 block);
TreeNode * uffs_FindErasedNodeByBlock(uffs_Device *dev, u16 block);
TreeNode * uffs_FindBadNodeByBlock(uffs_Device *dev, u16 block);

#define SEARCH_REGION_DIR		1
#define SEARCH_REGION_FILE		2
#define SEARCH_REGION_DATA		4
#define SEARCH_REGION_BAD		8
#define SEARCH_REGION_ERASED	16
TreeNode * uffs_FindNodeByBlock(uffs_Device *dev, u16 block, int *region);



UBOOL uffs_CompareFileNameWithTreeNode(uffs_Device *dev, const char *name, u32 len, u16 sum, TreeNode *node, int type);

TreeNode * uffs_GetErased(uffs_Device *dev);

void uffs_InsertNodeToTree(uffs_Device *dev, u8 type, TreeNode *node);
void uffs_InsertToErasedListHead(uffs_Device *dev, TreeNode *node);
void uffs_InsertToErasedListTail(uffs_Device *dev, TreeNode *node);
void uffs_InsertToBadBlockList(uffs_Device *dev, TreeNode *node);

void uffs_BreakFromEntry(uffs_Device *dev, u8 type, TreeNode *node);

void uffs_SetTreeNodeBlock(u8 type, TreeNode *node, u16 block);






#ifdef __cplusplus
}
#endif



#endif



/*
    Copyright (C) 2005-2008  Ricky Zheng <ricky_gz_zheng@yahoo.co.nz>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/
/**
 * \file uffs_tree.c
 * \brief seting up uffs tree data structure
 * \author Ricky Zheng, created 13th May, 2005
 */
#include "uffs/uffs_public.h"
#include "uffs/uffs_os.h"
#include "uffs/ubuffer.h"
#include "uffs/uffs_config.h"

#include <string.h>

#define PFX "tree: "

static void uffs_InsertToFileEntry(uffs_Device *dev, TreeNode *node);
static void uffs_InsertToDirEntry(uffs_Device *dev, TreeNode *node);
static void uffs_InsertToDataEntry(uffs_Device *dev, TreeNode *node);


/** 
 * \brief initialize tree buffers
 * \param[in] dev uffs device
 */
URET uffs_InitTreeBuf(uffs_Device *dev)
{
	int size;
	int nums;
	struct ubufm *dis;
	int i;

	size = sizeof(TreeNode);
	nums = dev->par.end - dev->par.start + 1;
	dis = &(dev->tree.dis);

	dis->node_size = size;
	dis->node_nums = nums;

	if (dev->mem.tree_buffer_size == 0) {
		if (dev->mem.malloc) {
			dev->mem.tree_buffer = dev->mem.malloc(dev, size * nums);
			if (dev->mem.tree_buffer) dev->mem.tree_buffer_size = size * nums;
		}
	}
	if (size * nums > dev->mem.tree_buffer_size) {
		uffs_Perror(UFFS_ERR_DEAD, PFX"Tree buffer require %d but only %d available.\n", size * nums, dev->mem.tree_buffer_size);
		memset(dis, 0, sizeof(struct ubufm));
		return U_FAIL;
	}
	dis->node_pool = dev->mem.tree_buffer;
	
	uBufInit(dis);
	dev->tree.erased = NULL;
	dev->tree.erased_tail = NULL;
	dev->tree.erasedCount = 0;
	dev->tree.bad = NULL;
	dev->tree.badCount = 0;

	for(i = 0; i < DIR_NODE_ENTRY_LEN; i++) {
		dev->tree.dirEntry[i] = EMPTY_NODE;
	}

	for(i = 0; i < FILE_NODE_ENTRY_LEN; i++) {
		dev->tree.fileEntry[i] = EMPTY_NODE;
	}

	for(i = 0; i < DATA_NODE_ENTRY_LEN; i++) {
		dev->tree.dataEntry[i] = EMPTY_NODE;
	}

	dev->tree.maxSerialNo = ROOT_DIR_ID;
	
	return U_SUCC;
}
/** 
 * \brief release tree buffers, call this function when unmount
 * \param[in] dev uffs device
 */
URET uffs_ReleaseTreeBuf(uffs_Device *dev)
{
	struct ubufm *dis;
	
	dis = &(dev->tree.dis);
	if(dis->node_pool && dev->mem.free) {
		dev->mem.free(dev, dis->node_pool);
	}
	uBufRelease(dis);
	memset(dis, 0, sizeof(struct ubufm));

	return U_SUCC;
}

static u16 _GetBlockFromNode(u8 type, TreeNode *node)
{
	switch(type){
	case UFFS_TYPE_DIR:
		return node->u.dir.block;
	case UFFS_TYPE_FILE:
		return node->u.file.block;
	case UFFS_TYPE_DATA:
		return node->u.data.block;
	}
	uffs_Perror(UFFS_ERR_SERIOUS, PFX"unkown type, X-block\n");
	return UFFS_INVALID_BLOCK;
}

#if 0
static u16 _GetFatherFromNode(u8 type, TreeNode *node)
{
	switch(type){
	case UFFS_TYPE_DIR:
		return node->u.dir.father;
	case UFFS_TYPE_FILE:
		return node->u.file.father;
	case UFFS_TYPE_DATA:
		return node->u.data.father;
	}
	uffs_Perror(UFFS_ERR_SERIOUS, PFX"unkown type, X-father\n");
	return INVALID_UFFS_SERIAL;
}


static u16 _GetSerialFromNode(u8 type, TreeNode *node)
{
	switch(type){
	case UFFS_TYPE_DIR:
		return node->u.dir.serial;
	case UFFS_TYPE_FILE:
		return node->u.file.serial;
	case UFFS_TYPE_DATA:
		return node->u.data.serial;
	}
	uffs_Perror(UFFS_ERR_SERIOUS, PFX"unkown type, X-serial\n");
	return INVALID_UFFS_SERIAL;
}
#endif

/** 
 * insert a TreeNode *node to tree
 * \param[in] dev uffs device
 * \param[in] type type of node
 * \param[in] node node to be insert to
 */
void uffs_InsertNodeToTree(uffs_Device *dev, u8 type, TreeNode *node)
{
	switch(type){
	case UFFS_TYPE_DIR:
		uffs_InsertToDirEntry(dev, node);
		break;
	case UFFS_TYPE_FILE:
		uffs_InsertToFileEntry(dev, node);
		break;
	case UFFS_TYPE_DATA:
		uffs_InsertToDataEntry(dev, node);
		break;
	default:
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"unkown type, can't insert to tree\n");
		break;
	}
}

/** 
 * find a node from tree
 * \param[in] dev uffs device
 * \param[in] type type of node
 * \param[in] father father serial num
 * \param[in] serial serial num
 */
TreeNode * uffs_FindFromTree(uffs_Device *dev, u8 type, u16 father, u16 serial)
{
	switch(type){
	case UFFS_TYPE_DIR:
		return uffs_FindDirNodeFromTree(dev, serial);
	case UFFS_TYPE_FILE:
		return uffs_FindFileNodeFromTree(dev, serial);
	case UFFS_TYPE_DATA:
		return uffs_FindDataNode(dev, father, serial);
	}
	uffs_Perror(UFFS_ERR_SERIOUS, PFX"unkown type, can't find node\n");
	return NULL;
}

static URET _BuildValidTreeNode(uffs_Device *dev,
								TreeNode *node,		//!< empty node
								uffs_blockInfo *bc)
{
	uffs_Tags *tag;
	TreeNode *node_alt;
	u16 block, father, serial, block_alt, block_save;
	uffs_blockInfo *bc_alt;
	u8 type;
	UBOOL needToInsertToTree = U_FALSE;
	//u16 page;

#if 0
	page = uffs_FindFirstValidPage(dev, bc); //@ may only read one spare: 0
#endif
	//@@@ Find first valid page ??? why not just load the first page ?
	//@@@ if the first page is invalid, that means ... the whole block is invalid !!! need to be erased !!!
	//@@@ if the first page is valid, well, let's keep going ...
	//@@@ do we need to check whether there is any 'dirty but invalid' pages here? no, no needed.
	//@@@ because 'uffs_FindBestPageInBlock' will skip the 'dirty but invalid' pages.

	uffs_LoadBlockInfo(dev, bc, 0);

	tag = &(bc->spares[0].tag);  //get first page's tag

	if (tag->dirty != TAG_DIRTY) {
		uffs_Perror(UFFS_ERR_NORMAL, "First page is clean in a non-erased block ?");
		return U_FAIL;
	}

	if (tag->valid == TAG_INVALID) {
//	if(page == UFFS_INVALID_PAGE) {

		//all pages are invalid ? should be erased now!
		uffs_Perror(UFFS_ERR_NORMAL, PFX"all pages in block %d are invalid, will be erased now!\n", bc->blockNum);
		
		dev->ops->EraseBlock(dev, bc->blockNum);
		uffs_ExpireBlockInfo(dev, bc, UFFS_ALL_PAGES);

		/* now, put this node to erased list to tail */
		uffs_InsertToErasedListTail(dev, node);
		return U_SUCC;
	}

#if 0
	page = uffs_FindBestPageInBlock(dev, bc, page); //@ FIX ME! read too many spares in here
	//@@@ Do we really need to 'find the best page' in the block ?
	//@@@ probably not :), because even the discarded page does have the sufficient information !
#endif

	//tag = &(bc->spares[page].tag);  //get first page's tag
	block = bc->blockNum;
	father = tag->father;
	serial = tag->serial;
	type = tag->type;

	//@ FIX ME! too heavy! especially when disk full...
	//@ To avoid to search the alternate node, a safe-power-off mark should be set when unmount uffs
	//@ It need to search the alternate node only when safe-power-off mark is not set.
	node_alt = uffs_FindFromTree(dev, type, father, serial); 

	if(node_alt != NULL) {
		//find a alternate node !

		block_alt = _GetBlockFromNode(type, node_alt);

		uffs_Perror(UFFS_ERR_NORMAL, PFX"Process unclean block (%d vs %d)\n", block, block_alt);

		if(block_alt == INVALID_UFFS_SERIAL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"invalid block ?\n");
			return U_FAIL;
		}
		
		bc_alt = uffs_GetBlockInfo(dev, block_alt);
		if(bc_alt == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get block info \n");
			return U_FAIL;
		}
		uffs_LoadBlockInfo(dev, bc_alt, 0);
		if(uffs_IsSrcNewerThanObj(
			tag->blockTimeStamp, 
			bc_alt->spares[0].tag.blockTimeStamp) == U_TRUE) {

			//the node is newer than node_alt, so keep node_alt, and erase node
			dev->ops->EraseBlock(dev, block);
			uffs_ExpireBlockInfo(dev, bc, UFFS_ALL_PAGES);
			node->u.list.block = block;
			uffs_InsertToErasedListTail(dev, node);
			uffs_PutBlockInfo(dev, bc_alt);

			return U_SUCC;
		}
		else {
			//the node is elder than node_alt, so keep node, and erase node_alt
			//we use node as erased node to insert to erased list

			block_save = _GetBlockFromNode(type, node_alt);
			dev->ops->EraseBlock(dev, block_save);
			uffs_ExpireBlockInfo(dev, bc_alt, UFFS_ALL_PAGES);
			node->u.list.block = block_save;
			uffs_InsertToErasedListTail(dev, node);
			uffs_PutBlockInfo(dev, bc_alt);
			
			node = node_alt;	//use node_alt to store new informations in following
			needToInsertToTree = U_FALSE;
		}
	}
	else {
		needToInsertToTree = U_TRUE;
	}

	switch(type) {
	case UFFS_TYPE_DIR:
		node->u.dir.block = bc->blockNum;
		node->u.dir.checkSum = tag->dataSum;
		node->u.dir.father = tag->father;
		node->u.dir.serial = tag->serial;
		//node->u.dir.pagID = tag->pageID;
		//node->u.dir.ofs = (u8)page;
		break;
	case UFFS_TYPE_FILE:
		node->u.file.block = bc->blockNum;
		node->u.file.checkSum = tag->dataSum;
		node->u.file.father = tag->father;
		node->u.file.serial = tag->serial;
		node->u.file.len = uffs_GetBlockFileDataLength(dev, bc, UFFS_TYPE_FILE);  
		break;
	case UFFS_TYPE_DATA:
		node->u.data.block = bc->blockNum;
		node->u.data.father = tag->father;
		node->u.data.serial = tag->serial;
		if(tag->serial == 1) {
			tag->serial = tag->serial;
		}
		node->u.data.len = uffs_GetBlockFileDataLength(dev, bc, UFFS_TYPE_DATA); 
		break;
	}

	if(needToInsertToTree == U_TRUE) {
		uffs_InsertNodeToTree(dev, type, node);
	}

	return U_SUCC;
}

static URET _BuildTreeStepOne(uffs_Device *dev)
{
	int block_lt;
	uffs_blockInfo *bc;
	TreeNode *node;
	struct uffs_treeSt *tree;
	struct ubufm *dis;
	URET ret = U_SUCC;
	
	tree = &(dev->tree);
	dis = &(tree->dis);

	tree->bad = NULL;
	tree->badCount = 0;
	tree->erased = NULL;
	tree->erased_tail = NULL;
	tree->erasedCount = 0;

	uffs_Perror(UFFS_ERR_NOISY, PFX"build tree step one\n");

//	printf("s:%d e:%d\n", dev->par.start, dev->par.end);
	for(block_lt = dev->par.start; block_lt <= dev->par.end; block_lt++) {
		bc = uffs_GetBlockInfo(dev, block_lt);
//		uffs_Perror(UFFS_ERR_NORMAL, PFX"loop\n");
		if(bc == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"step one:fail to get block info\n");
			ret = U_FAIL;
			break;
		}
		node = (TreeNode *)uBufGet(dis);
		if(node == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"insufficient tree node!\n");
			ret = U_FAIL;
			break;
		}
		//Need to check bad block at first !
		if(dev->flash->IsBlockBad(dev, bc) == U_TRUE) { //@ read one spare: 0
			node->u.list.block = block_lt;
			uffs_InsertToBadBlockList(dev, node);
			uffs_Perror(UFFS_ERR_NORMAL, PFX"found bad block %d\n", block_lt);
		}
		else if(uffs_IsPageErased(dev, bc, 0) == U_TRUE) { //@ read one spare: 0
			//just need to check page 0 to judge whether the block is erased
			node->u.list.block = block_lt;
			uffs_InsertToErasedListTail(dev, node);
		}
		else {
			//uffs_Perror(UFFS_ERR_NOISY, PFX"find a valid block\n");
			ret = _BuildValidTreeNode(dev, node, bc);
			//uffs_Perror(UFFS_ERR_NOISY, PFX"valid block done!\n");
			if(ret == U_FAIL) break;
		}
		uffs_PutBlockInfo(dev, bc);
	} //end of for

	if(ret == U_FAIL) uffs_PutBlockInfo(dev, bc);

	return ret;
}

static URET _BuildTreeStepTwo(uffs_Device *dev)
{
	//Random the start point of erased block to implement ware leveling
	u32 startCount = 0;
	u32 endPoint;
	TreeNode *node;

	uffs_Perror(UFFS_ERR_NOISY, PFX"build tree step two\n");

	endPoint = uffs_GetCurDateTime() % dev->tree.erasedCount;
	while(startCount < endPoint) {
		node = uffs_GetErased(dev);
		if(node == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"No erased block ?\n");
			return U_FAIL;
		}
		uffs_InsertToErasedListTail(dev, node);
		startCount++;
	}
	return U_SUCC;
}

TreeNode * uffs_FindFileNodeFromTree(uffs_Device *dev, u16 serial)
{
	int hash;
	u16 x;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);

	hash = serial & FILE_NODE_HASH_MASK;
	x = tree->fileEntry[hash];
	while(x != EMPTY_NODE) {
		node = FROM_IDX(x, &(tree->dis));
		if(node->u.file.serial == serial) {
			return node;
		}
		else {
			x = node->hashNext;
		}
	}
	return NULL;
}

TreeNode * uffs_FindFileNodeFromTreeWithFather(uffs_Device *dev, u16 father)
{
	int hash;
	u16 x;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);

	for (hash = 0; hash < FILE_NODE_ENTRY_LEN; hash++) {
		x = tree->fileEntry[hash];
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(tree->dis));
			if(node->u.file.father == father) {
				return node;
			}
			else {
				x = node->hashNext;
			}
		}
	}
	return NULL;
}

TreeNode * uffs_FindDirNodeFromTree(uffs_Device *dev, u16 serial)
{
	int hash;
	u16 x;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);

	hash = serial & DIR_NODE_HASH_MASK;
	x = tree->dirEntry[hash];
	while(x != EMPTY_NODE) {
		node = FROM_IDX(x, &(tree->dis));
		if(node->u.dir.serial == serial) {
			return node;
		}
		else {
			x = node->hashNext;
		}
	}
	return NULL;
}

TreeNode * uffs_FindDirNodeFromTreeWithFather(uffs_Device *dev, u16 father)
{
	int hash;
	u16 x;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);

	for (hash = 0; hash < DIR_NODE_ENTRY_LEN; hash++) {
		x = tree->dirEntry[hash];
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(tree->dis));
			if(node->u.dir.father == father) {
				return node;
			}
			else {
				x = node->hashNext;
			}
		}
	}
	
	return NULL;
}

TreeNode * uffs_FindFileNodeByName(uffs_Device *dev, const char *name, u32 len, u16 sum, u16 father)
{
	int i;
	u16 x;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);
	
	for(i = 0; i < FILE_NODE_ENTRY_LEN; i++) {
		x = tree->fileEntry[i];
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(tree->dis));
			if(node->u.file.checkSum == sum && node->u.file.father == father) {
				//read file name from flash, and compare...
				if(uffs_CompareFileNameWithTreeNode(dev, name, len, sum, node, UFFS_TYPE_FILE) == U_TRUE) {
					//Got it!
					return node;
				}
			}
			x = node->hashNext;
		}
	}
	return NULL;
}

TreeNode * uffs_FindDataNode(uffs_Device *dev, u16 father, u16 serial)
{
	int hash;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);
	u16 x;

	hash = GET_DATA_HASH(father, serial);
	x = tree->dataEntry[hash];
	while(x != EMPTY_NODE) {
		node = FROM_IDX(x, &(tree->dis));

		if(node->u.data.father == father &&
			node->u.data.serial == serial)
				return node;

		x = node->hashNext;
	}
	return NULL;
}

TreeNode * uffs_FindDirNodeByBlock(uffs_Device *dev, u16 block)
{
	int hash;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);
	u16 x;

	for(hash = 0; hash < DIR_NODE_ENTRY_LEN; hash++) {
		x = tree->dirEntry[hash];
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(tree->dis));
			if(node->u.dir.block == block)
				return node;
			x = node->hashNext;
		}
	}
	return NULL;
}

TreeNode * uffs_FindErasedNodeByBlock(uffs_Device *dev, u16 block)
{
	TreeNode *node;
	node = dev->tree.erased;
	while(node) {
		if(node->u.list.block == block) return node;
		node = node->u.list.next;
	}
		
	return NULL;
}

TreeNode * uffs_FindBadNodeByBlock(uffs_Device *dev, u16 block)
{
	TreeNode *node;
	node = dev->tree.bad;
	while(node) {
		if(node->u.list.block == block) return node;
		node = node->u.list.next;
	}
		
	return NULL;
}

TreeNode * uffs_FindFileNodeByBlock(uffs_Device *dev, u16 block)
{
	int hash;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);
	u16 x;

	for(hash = 0; hash < FILE_NODE_ENTRY_LEN; hash++) {
		x = tree->fileEntry[hash];
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(tree->dis));
			if(node->u.file.block == block)
				return node;
			x = node->hashNext;
		}
	}
	return NULL;
}

TreeNode * uffs_FindDataNodeByBlock(uffs_Device *dev, u16 block)
{
	int hash;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);
	u16 x;

	for(hash = 0; hash < DATA_NODE_ENTRY_LEN; hash++) {
		x = tree->dataEntry[hash];
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(tree->dis));
			if(node->u.data.block == block)
				return node;
			x = node->hashNext;
		}
	}
	return NULL;
}

TreeNode * uffs_FindNodeByBlock(uffs_Device *dev, u16 block, int *region)
{
	TreeNode *node = NULL;
	if(*region & SEARCH_REGION_DATA) {
		node = uffs_FindDataNodeByBlock(dev, block);
		if(node) {
			*region &= SEARCH_REGION_DATA;
			return node;
		}
	}
	if(*region & SEARCH_REGION_FILE) {
		node = uffs_FindFileNodeByBlock(dev, block);
		if(node) {
			*region &= SEARCH_REGION_FILE;
			return node;
		}
	}
	if(*region & SEARCH_REGION_DIR) {
		node = uffs_FindDirNodeByBlock(dev, block);
		if(node) {
			*region &= SEARCH_REGION_DIR;
			return node;
		}
	}
	if(*region & SEARCH_REGION_ERASED) {
		node = uffs_FindErasedNodeByBlock(dev, block);
		if(node) {
			*region &= SEARCH_REGION_ERASED;
			return node;
		}
	}
	if(*region & SEARCH_REGION_BAD) {
		node = uffs_FindBadNodeByBlock(dev, block);
		if(node) {
			*region &= SEARCH_REGION_BAD;
			return node;
		}
	}
	return node;
}

TreeNode * uffs_FindDirNodeByName(uffs_Device *dev, const char *name, u32 len, u16 sum, u16 father)
{
	int i;
	u16 x;
	TreeNode *node;
	struct uffs_treeSt *tree = &(dev->tree);
	
	for(i = 0; i < DIR_NODE_ENTRY_LEN; i++) {
		x = tree->dirEntry[i];
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(tree->dis));
			if(node->u.dir.checkSum == sum && node->u.dir.father == father) {
				//read file name from flash, and compare...
				if(uffs_CompareFileNameWithTreeNode(dev, name, len, sum, node, UFFS_TYPE_DIR) == U_TRUE) {
					//Got it!
					return node;
				}
			}
			x = node->hashNext;
		}
	}
	return NULL;

}

UBOOL uffs_CompareFileName(const char *src, int src_len, const char *des)
{
	while(src_len-- > 0) {
		if(*src++ != *des++) return U_FALSE;
	}
	return U_TRUE;
}

UBOOL uffs_CompareFileNameWithTreeNode(uffs_Device *dev, const char *name, u32 len, u16 sum, TreeNode *node, int type)
{
	u16 page;
	uffs_blockInfo *bc;
	uffs_Tags *tag;
	URET ret = U_FAIL;
	uffs_fileInfo fi;
	u16 block;

	if (type == UFFS_TYPE_DIR)
		block = node->u.dir.block;
	else
		block = node->u.file.block;

	bc = uffs_GetBlockInfo(dev, block);

	if(bc == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get block info %d\n", block);
		return U_FALSE;
	}

	//dir|file name must resident in pageID == 0
	uffs_LoadBlockInfo(dev, bc, UFFS_ALL_PAGES);
	page = uffs_FindBestPageInBlock(dev, bc, 0);

	tag = &(bc->spares[page].tag);
	if(tag->dataSum != sum) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"the obj's sum in tag is different with given sum!\n");
		uffs_PutBlockInfo(dev, bc);
		return U_FALSE;
	}

	ret = dev->ops->ReadPageData(dev, block, page, (u8 *)(&fi), 0, sizeof(uffs_fileInfo));
	if(ret == U_FAIL) {
		uffs_PutBlockInfo(dev, bc);
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"I/O error ?\n");
		return U_FALSE;
	}

	if(fi.name_len == len) {
		if(uffs_CompareFileName(fi.name, fi.name_len, name) == U_TRUE) {
			uffs_PutBlockInfo(dev, bc);
			return U_TRUE;
		}
	}

	uffs_PutBlockInfo(dev, bc);
	return U_FALSE;
}


/* calculate file length */
static URET _BuildTreeStepThree(uffs_Device *dev)
{
	int i;
	u16 x;
	TreeNode *work;
	TreeNode *node;
	struct uffs_treeSt *tree;
	struct ubufm *dis;
	u16 blockSave;

	TreeNode *cache = NULL;
	u16 cacheSerial = INVALID_UFFS_SERIAL;


	//FIX ME!! a cache system MUST be designed to accelerate this procedure
	//FIX ME!! otherwise a huge file nodes or data nodes would drag system from fast booting

	tree = &(dev->tree);
	dis = &(tree->dis);

	uffs_Perror(UFFS_ERR_NOISY, PFX"build tree step three\n");

	for(i = 0; i < DATA_NODE_ENTRY_LEN; i++) {
		x = tree->dataEntry[i];
		while(x != EMPTY_NODE) {
			work = FROM_IDX(x, dis);
			if(work->u.data.father == cacheSerial) {
				node = cache;
			}
			else {
				node = uffs_FindFileNodeFromTree(dev, work->u.data.father);
				cache = node;
				cacheSerial = work->u.data.father;
			}
			if(node == NULL) {
				x = work->hashNext;
				//this data block do not belong any file ?
				//should be erased.
				uffs_Perror(UFFS_ERR_NORMAL, PFX"find a orphan data block:%d, father:%d, serial:%d, will be erased!\n", 
					work->u.data.block, work->u.data.father, work->u.data.serial);
				uffs_BreakFromEntry(dev, UFFS_TYPE_DATA, work);
				blockSave = work->u.data.block;
				work->u.list.block = blockSave;
				dev->ops->EraseBlock(dev, blockSave);
				uffs_InsertToErasedListTail(dev, work);
			}
			else {
				node->u.file.len += work->u.data.len;
				x = work->hashNext;
			}
		}
	}

	return U_SUCC;
}

/** 
 * \brief build tree structure from flash
 * \param[in] dev uffs device
 */
URET uffs_BuildTree(uffs_Device *dev)
{
	URET ret;

	/***** step one: scan all page spares, classify DIR/FILE/DATA nodes,
			check bad blocks/uncompleted(conflicted) blocks as well *****/
	/* if the disk is big and full filled of data this step could be the most time consuming .... */
	ret = _BuildTreeStepOne(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"build tree step one fail!\n");
		return ret;
	}

	/***** step two: randomize the erased blocks, for ware-leveling purpose *****/
	/* this step is very fast :) */
	ret = _BuildTreeStepTwo(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"build tree step two fail!\n");
		return ret;
	}

	/***** step three: check DATA nodes, find orphan nodes and erase them *****/
	/* if there are a lot of files and disk is fully filled, this step could be very time consuming ... */
	ret = _BuildTreeStepThree(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"build tree step three fail!\n");
		return ret;
	}
	
	return U_SUCC;
}

/** 
 * find a free file or dir serial NO
 * \param[in] dev uffs device
 * \return if no free serial found, return #INVALID_UFFS_SERIAL
 */
u16 uffs_FindFreeFsnSerial(uffs_Device *dev)
{
	u16 i;
	TreeNode *node;

	//TODO!! Do we need a faster serial number generating method?
	//		 it depends on how often creating files or directories

	for(i = ROOT_DIR_ID + 1; i < MAX_UFFS_SERIAL; i++) {
		node = uffs_FindDirNodeFromTree(dev, i);
		if(node == NULL) {
			node = uffs_FindFileNodeFromTree(dev, i);
			if(node == NULL) return i;
		}
	}
	return INVALID_UFFS_SERIAL;
}

TreeNode * uffs_GetErased(uffs_Device *dev)
{
	TreeNode *node = NULL;
	if(dev->tree.erased) {
		node = dev->tree.erased;
		dev->tree.erased->u.list.prev = NULL;
		dev->tree.erased = dev->tree.erased->u.list.next;
		if(dev->tree.erased == NULL) dev->tree.erased_tail = NULL;
	}
	dev->tree.erasedCount--;

	return node;
}

static void _InsertToEntry(uffs_Device *dev, u16 *entry, int hash, TreeNode *node)
{
	struct uffs_treeSt *tree = &(dev->tree);

	node->hashNext = entry[hash];
#ifdef TREE_NODE_USE_DOUBLE_LINK
	node->hashPrev = EMPTY_NODE;
	if(entry[hash] != EMPTY_NODE) {
		FROM_IDX(entry[hash], &(tree->dis))->hashPrev = TO_IDX(node, &(tree->dis));
	}
#endif
	entry[hash] = TO_IDX(node, &(tree->dis));
}

//static void _BreakFromEntry(uffs_Device *dev, u16 *entry, int hash, TreeNode *node)
//{
//	TreeNode *work;
//	struct uffs_treeSt *tree = &(dev->tree);
//	struct ubufm *dis = &(tree->dis);
//	
//	if(node->hashPrev != EMPTY_NODE) {
//		work = FROM_IDX(node->hashPrev, dis);
//		work->hashNext = node->hashNext;
//	}
//	if(node->hashNext != EMPTY_NODE) {
//		work = FROM_IDX(node->hashNext, dis);
//		work->hashPrev = node->hashPrev;
//	}
//	if(entry[hash] == TO_IDX(node, dis)) {
//		entry[hash] = node->hashNext;
//	}
//}

#ifndef TREE_NODE_USE_DOUBLE_LINK
TreeNode * _FindPrevNodeFromEntry(uffs_Device *dev, u16 start, u16 find)
{
	TreeNode *work;
	while (start != EMPTY_NODE) {
		work = FROM_IDX(start, &(dev->tree.dis));
		if (work->hashNext == find) {
			return work;
		}
	}
	return NULL;
}
#endif

/** 
 * break the node from entry
 */
void uffs_BreakFromEntry(uffs_Device *dev, u8 type, TreeNode *node)
{
	u16 *entry;
	int hash;
	TreeNode *work;

	switch(type) {
	case UFFS_TYPE_DIR:
		hash = GET_DIR_HASH(node->u.dir.serial);
		entry = &(dev->tree.dirEntry[hash]);
		break;
	case UFFS_TYPE_FILE:
		hash = GET_FILE_HASH(node->u.file.serial);
		entry = &(dev->tree.fileEntry[hash]);
		break;
	case UFFS_TYPE_DATA:
		hash = GET_DATA_HASH(node->u.data.father, node->u.data.serial);
		entry = &(dev->tree.dataEntry[hash]);
		break;
	default:
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"unknown type when break...\n");
		return;
	}
#ifdef TREE_NODE_USE_DOUBLE_LINK
	if(node->hashPrev != EMPTY_NODE) {
		work = FROM_IDX(node->hashPrev, &(dev->tree.dis));
		work->hashNext = node->hashNext;
	}
	if(node->hashNext != EMPTY_NODE) {
		work = FROM_IDX(node->hashNext, &(dev->tree.dis));
		work->hashPrev = node->hashPrev;
	}
#else
	if ((work = _FindPrevNodeFromEntry(dev, *entry, TO_IDX(node, &(dev->tree.dis)))) != NULL) {
		work->hashNext = node->hashNext;
	}
#endif

	if(*entry == TO_IDX(node, &(dev->tree.dis))) {
		*entry = node->hashNext;
	}
}

static void uffs_InsertToFileEntry(uffs_Device *dev, TreeNode *node)
{
	_InsertToEntry(dev, dev->tree.fileEntry, GET_FILE_HASH(node->u.file.serial), node);
}

static void uffs_InsertToDirEntry(uffs_Device *dev, TreeNode *node)
{
	_InsertToEntry(dev, dev->tree.dirEntry, GET_DIR_HASH(node->u.dir.serial), node);
}

static void uffs_InsertToDataEntry(uffs_Device *dev, TreeNode *node)
{
	_InsertToEntry(dev, dev->tree.dataEntry, GET_DATA_HASH(node->u.data.father, node->u.data.serial), node);
}

void uffs_InsertToErasedListHead(uffs_Device *dev, TreeNode *node)
{
	struct uffs_treeSt *tree;
	tree = &(dev->tree);

	node->u.list.next = tree->erased;
	node->u.list.prev = NULL;
	if(tree->erased) {
		tree->erased->u.list.prev = node;
	}

	tree->erased = node;
	if(node->u.list.next == tree->erased_tail) {
		tree->erased_tail = node;
	}
	tree->erasedCount++;
}

void uffs_InsertToErasedListTail(uffs_Device *dev, TreeNode *node)
{
	struct uffs_treeSt *tree;
	tree = &(dev->tree);

	node->u.list.next = NULL;
	node->u.list.prev = tree->erased_tail;
	if(tree->erased_tail) {
		tree->erased_tail->u.list.next = node;
	}

	tree->erased_tail = node;
	if(tree->erased == NULL) {
		tree->erased = node;
	}
	tree->erasedCount++;
}

void uffs_InsertToBadBlockList(uffs_Device *dev, TreeNode *node)
{
	struct uffs_treeSt *tree;
	tree = &(dev->tree);
	node->u.list.prev = NULL;
	node->u.list.next = tree->bad;
	if(tree->bad) {
		tree->bad->u.list.prev = node;
	}
	tree->bad = node;
	tree->badCount++;
}

/** 
 * set tree node block value
 */
void uffs_SetTreeNodeBlock(u8 type, TreeNode *node, u16 block)
{
	switch(type) {
	case UFFS_TYPE_FILE:
		node->u.file.block = block;
		break;
	case UFFS_TYPE_DIR:
		node->u.dir.block = block;
		break;
	case UFFS_TYPE_DATA:
		node->u.data.block = block;
		break;
	}
}


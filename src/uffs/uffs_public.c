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
 * \file uffs_public.c
 * \brief public and miscellaneous functions
 * \author Ricky Zheng, created 10th May, 2005
 */

#include "uffs/uffs_types.h"
#include "uffs/uffs_config.h"
#include "uffs/uffs_core.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_os.h"

#include <string.h>

#define PFX "pub: "


int uffs_GetFirstBlockTimeStamp(void)
{
	return 0;
}

int uffs_GetNextBlockTimeStamp(int prev)
{
	return (prev + 1) % 3;
}

UBOOL uffs_IsSrcNewerThanObj(int src, int obj)
{
	switch(src - obj) {
		case 0:
			uffs_Perror(UFFS_ERR_SERIOUS, PFX "the two block have the same time stamp ?\n");
			break;
		case 1:
		case -2:
			return U_TRUE;
		case -1:
		case 2:
			return U_FALSE;
		default:
			uffs_Perror(UFFS_ERR_SERIOUS, PFX "time stamp out of range !\n");
			break;
	}
	return U_FALSE;
}


//URET uffs_ECCCheck(uffs_Device *dev, uffs_Buf *buf)
//{
//	dev = dev;
//	return U_SUCC;
//}

/** 
 * \brief Calculate tag checksum
 * \param[in] tag input tag
 * \return checksum of tag
 */
u8 uffs_CalTagCheckSum(uffs_Tags *tag)
{
#if defined(ENABLE_TAG_CHECKSUM) && ENABLE_TAG_CHECKSUM == 1
	u8 checkSum = 0;
	int i;
	int ofs;

	ofs = (int)(&(((uffs_Tags *)NULL)->checkSum));

	for(i = 0; i < ofs; i++) {
		checkSum += *((u8 *)(tag) + i);
	}
	return checkSum;
#else
	tag = tag;
	return 0xff;
#endif
}


/** 
 * \brief find a best page than given page from page.
 * \param[in] dev uffs device
 * \param[in] bc block info
 * \param[in] page page number to be compared with
 * \return the better page number, could be the same with given page
 */
u16 uffs_FindBestPageInBlock(uffs_Device *dev, uffs_blockInfo *bc, u16 page)
{
	uffs_pageSpare *spare_old, *spare;
	int i;
	int best;

	if(page == dev->attr->pages_per_block - 1) return page;
	
	uffs_LoadBlockInfo(dev, bc, page); //load old page
	spare_old = &(bc->spares[page]);

	if(spare_old->tag.pageID == page) {
		//well, try to speed up ....
		uffs_LoadBlockInfo(dev, bc, dev->attr->pages_per_block - 1); 
		spare = &(bc->spares[dev->attr->pages_per_block - 1]);
		if(spare->tag.pageID == dev->attr->pages_per_block - 1) {
			return page;
		}
	}

	uffs_LoadBlockInfo(dev, bc, UFFS_ALL_PAGES);
	best = page;
	//the better page must be ahead of page, so ...i = page + 1; i < ...
	for(i = page + 1; i < dev->attr->pages_per_block; i++) {
		spare = &(bc->spares[i]);
		if(spare->tag.pageID == spare_old->tag.pageID) {
			if(	spare->tag.father == spare_old->tag.father &&
				spare->tag.serial == spare_old->tag.serial &&
				spare->tag.dirty == TAG_DIRTY && //0: dirty, 1:clear
				spare->tag.valid == TAG_VALID) { //0: valid, 1:invalid
				if(i > best) best = i;
			}
		}
	}
	return best;
}

/** 
 * \brief find a valid page with given pageID
 * \param[in] dev uffs device
 * \param[in] bc block info
 * \param[in] pageID pageID to be find
 * \return the valid page number which has given pageID
 * \retval >=0 page number
 * \retval UFFS_INVALID_PAGE page not found
 */
u16 uffs_FindPageInBlockWithPageId(uffs_Device *dev, uffs_blockInfo *bc, u16 pageID)
{
	u16 page;
	uffs_Tags *tag;

	//Indeed, the page which has pageID, should ahead of pageID ...
	for(page = pageID; page < dev->attr->pages_per_block; page++) {
		uffs_LoadBlockInfo(dev, bc, page);
		tag = &(bc->spares[page].tag);
		if(tag->pageID == pageID) return page;
	}
	return UFFS_INVALID_PAGE;
}

/** 
 * Are all the pages in the block used ?
 */
UBOOL uffs_IsBlockPagesFullUsed(uffs_Device *dev, uffs_blockInfo *bc)
{
	uffs_LoadBlockInfo(dev, bc, dev->attr->pages_per_block - 1);
	if(bc->spares[dev->attr->pages_per_block - 1].tag.dirty == TAG_DIRTY) return U_TRUE;

	return U_FALSE;
}

/** 
 * Is this block used ?
 * \param[in] dev uffs device
 * \param[in] bc block info
 * \retval U_TRUE block is used
 * \retval U_FALSE block is free
 */
UBOOL uffs_IsThisBlockUsed(uffs_Device *dev, uffs_blockInfo *bc)
{
	uffs_LoadBlockInfo(dev, bc, 0);
	if(bc->spares[0].tag.dirty == TAG_DIRTY) return U_TRUE;

	return U_FALSE;
}

/** 
 * get block time stamp from a exist block
 * \param[in] dev uffs device
 * \param[in] bc block info
 */
int uffs_GetBlockTimeStamp(uffs_Device *dev, uffs_blockInfo *bc)
{
	if(uffs_IsThisBlockUsed(dev, bc) == U_FALSE) 
		return uffs_GetFirstBlockTimeStamp();
	else{
		uffs_LoadBlockInfo(dev, bc, 0);
		return bc->spares[0].tag.blockTimeStamp;
	}

}

/** 
 * find first free page from 'pageFrom'
 * \param[in] dev uffs device
 * \param[in] bc block info
 * \param[in] pageFrom search from this page
 * \return return first free page number from 'pageFrom'
 * \retval UFFS_INVALID_PAGE no free page found
 * \retval >=0 the first free page number
 */
u16 uffs_FindFirstFreePage(uffs_Device *dev, uffs_blockInfo *bc, u16 pageFrom)
{
	u16 i;

	for(i = pageFrom; i < dev->attr->pages_per_block; i++) {
		uffs_LoadBlockInfo(dev, bc, i);
		if(uffs_IsPageErased(dev, bc, i) == U_TRUE)
			return i;
	}
	return UFFS_INVALID_PAGE; //free page not found
}


/** 
 * Find first valid page from a block, just used in mounting a partition
 */
u16 uffs_FindFirstValidPage(uffs_Device *dev, uffs_blockInfo *bc)
{
	u16 i;
	for(i = 0; i < dev->attr->pages_per_block; i++) {
		uffs_LoadBlockInfo(dev, bc, i);
		if(bc->spares[i].checkOk) return i;
	}
	return UFFS_INVALID_PAGE;
}

/** 
 * write data to a new page
 * \param[in] dev uffs device
 * \param[in] block block number to be wrote to
 * \param[in] page page number to be wrote to
 * \param[in] tag new page tag
 * \param[in] buf new page data
 */
URET uffs_WriteDataToNewPage(uffs_Device *dev, 
							 u16 block, 
							 u16 page,
							 uffs_Tags *tag,
							 uffs_Buf *buf)
{
	URET ret = U_SUCC;

	tag->dirty = 0;
	tag->valid = 1;
#if defined(ENABLE_TAG_CHECKSUM) && ENABLE_TAG_CHECKSUM == 1
	tag->checkSum = 0xff;
#endif

//	uffs_Perror(UFFS_ERR_NOISY, PFX"write b:%d p:%d t:%d f:%d s:%d id:%d L:%d\n",
//				block, page, buf->type, buf->father, buf->serial, buf->pageID, buf->dataLen);

	//step 1: write spare
	ret = dev->flash->WritePageSpare(dev, block, page, tag);
	if(ret != U_SUCC)
		return ret;
	
	//step 2: write page data
	dev->flash->MakeEcc(dev, buf->data, buf->ecc);
	ret = dev->ops->WritePageData(dev, block, page, buf->data, 0, dev->com.pgSize);
	if(ret != U_SUCC)
		return ret;

	//step 3: write spare again, make page valid
	tag->valid = 0;
#if defined(ENABLE_TAG_CHECKSUM) && ENABLE_TAG_CHECKSUM == 1
	tag->checkSum = uffs_CalTagCheckSum(tag); //calculate right check sum
#endif
	ret = dev->flash->MakePageValid(dev, block, page, tag);
	if(ret != U_SUCC)
		return ret;

	return U_SUCC;
}

///** 
// * \brief recover a page in block
// * \param[in] dev uffs device
// * \parma[in] bc block information buffer
// * \param[in] oldPage old page number
// * \param[in] newPage new page number
// * \param[in] buf new page data buffer
// * \note the new data and length should be set to buf before this function invoked
// */
//URET uffs_PageRecover(uffs_Device *dev, 
//					  uffs_blockInfo *bc, 
//					  int oldPage, 
//					  int newPage, 
//					  uffs_Buf *buf)
//{
//	uffs_Tags *oldTag, *newTag;
//	uffs_pageSpare *newSpare;
//
//	if(newPage < 0 || newPage >= dev->attr->pages_per_block) {
//		uffs_Perror(UFFS_ERR_SERIOUS, PFX "new page number outof range!\n");
//		return U_FAIL;
//	}
//
//	uffs_LoadBlockInfo(dev, bc, oldPage);
//	uffs_LoadBlockInfo(dev, bc, newPage);
//
//	oldTag = &(bc->spares[oldPage].tag);	
//	newTag = &(bc->spares[newPage].tag);	
//
//	newSpare = &(bc->spares[newPage]);
//
//	newSpare->expired = 1; // make it expired firstly
//
//	newTag->fdn = oldTag->fdn;
//	newTag->blockTimeStamp = oldTag->blockTimeStamp;
//	newTag->fsn = oldTag->fsn;
//	newTag->pageID = oldTag->pageID;
//	newTag->dirty = 0;
//	newTag->dataLength = buf->dataLen;
//	newTag->checkSum = 0xff; //set check sum with 0xff first.
//	newTag->valid = 1;
//
//	uffs_WriteDataToNewPage(dev, bc->blockNum, newPage, newTag, buf);
//
//	return U_SUCC;
//}

/** 
 * calculate sum of data, 8bit version
 * \param[in] p data pointer
 * \param[in] len length of data
 * \return return sum of data, 8bit
 */
u8 uffs_MakeSum8(void *p, int len)
{
	u8 ret = 0;
	u8 *data = (u8 *)p;
	if (!p) return 0;
	while(len > 0) {
		ret += *data++;
		len--;
	}
	return ret;
}

/** 
 * calculate sum of datam, 16bit version
 * \param[in] p data pointer
 * \param[in] len length of data
 * \return return sum of data, 16bit
 */
u16 uffs_MakeSum16(void *p, int len)
{
	u8 ret_lo = 0;
	u8 ret_hi = 0;
	u8 *data = (u8 *)p;
	if (!p) return 0;
	while(len > 0) {
		ret_lo += *data;
		ret_hi ^= *data;
		data++;
		len--;
	}
	return (ret_hi << 8) | ret_lo;
}

/** 
 * create a new file on a free block
 * \param[in] dev uffs device
 * \param[in] father father dir serial num
 * \param[in] serial serial num of this new file
 * \param[in] bc block information
 * \param[in] fi file information
 * \note father, serial, bc must be provided before, and all information in fi should be filled well before.
 */
URET uffs_CreateNewFile(uffs_Device *dev, u16 father, u16 serial, uffs_blockInfo *bc, uffs_fileInfo *fi)
{
	uffs_Tags *tag;
	uffs_Buf *buf;

	fi->createTime = fi->lastModify = uffs_GetCurDateTime();

	uffs_LoadBlockInfo(dev, bc, 0);

	tag = &(bc->spares[0].tag);
	tag->father = father;
	tag->serial = serial;
	tag->dataLength = sizeof(uffs_fileInfo);
	tag->dataSum = uffs_MakeSum16(fi->name, fi->name_len);

	buf = uffs_BufGet(dev, father, serial, 0);
	if(buf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"get buf fail.\n");
		return U_FAIL;
	}

	memcpy(buf->data, fi, tag->dataLength);
	buf->dataLen = tag->dataLength;

	return uffs_BufPut(dev, buf);
}


/** 
 * \brief calculate data length of a file block
 * \param[in] dev uffs device
 * \param[in] bc block info
 */
int uffs_GetBlockFileDataLength(uffs_Device *dev, uffs_blockInfo *bc, u8 type)
{
	u16 pageID;
	u16 i;
	uffs_Tags *tag;
	int size = 0;
	u16 page;
	u16 lastPage = dev->attr->pages_per_block - 1;

	// TODO: Need to speed up this procedure!
	// First try the last page. will hit it if it's the full loaded file/data block.
	uffs_LoadBlockInfo(dev, bc, lastPage);
	tag = &(bc->spares[lastPage].tag);

	if(type == UFFS_TYPE_FILE) {
		if(tag->pageID == (lastPage - 1) &&
			tag->dataLength == dev->com.pgDataSize) {
			size = dev->com.pgDataSize * (dev->attr->pages_per_block - 1);
			return size;
		}
	}
	if(type == UFFS_TYPE_DATA) {
		if(tag->pageID == lastPage &&
			tag->dataLength == dev->com.pgDataSize) {
			size = dev->com.pgDataSize * dev->attr->pages_per_block;
			return size;
		}
	}

	// ok, it's not the full loaded file/data block, need to read all spares....
	uffs_LoadBlockInfo(dev, bc, UFFS_ALL_PAGES);
	tag = &(bc->spares[0].tag);
	if(tag->type == UFFS_TYPE_FILE) {
		pageID = 1; //In file header block, file data pageID from 1
		i = 1;		//search from page 1
	}
	else {
		pageID = 0;	//in normal file data block, pageID from 0
		i = 0;		//in normal file data block, search from page 0
	}
	for(; i < dev->attr->pages_per_block; i++) {
		tag = &(bc->spares[i].tag);
		if(pageID == tag->pageID) {
			page = uffs_FindBestPageInBlock(dev, bc, i);
			size += bc->spares[page].tag.dataLength;
			pageID++;
		}
	}
	return size;
}

/** 
 * get free pages number
 * \param[in] dev uffs device
 * \param[in] bc block info
 */
int uffs_GetFreePagesCount(uffs_Device *dev, uffs_blockInfo *bc)
{
	int count = 0;
	int i;

	for(i = dev->attr->pages_per_block - 1; i >= 0; i--) {
		uffs_LoadBlockInfo(dev, bc, i);
		if(uffs_IsPageErased(dev, bc, (u16)i) == U_TRUE) {
			count++;
		}
		else break;
	}
	return count;
}
/** 
 * \brief Is the block erased ?
 * \param[in] dev uffs device
 * \param[in] bc block info
 * \param[in] page page number to be check
 * \retval U_TRUE block is erased, ready to use
 * \retval U_FALSE block is dirty, maybe use by file
 */
UBOOL uffs_IsPageErased(uffs_Device *dev, uffs_blockInfo *bc, u16 page)
{
	uffs_Tags *tag;

	uffs_LoadBlockInfo(dev, bc, page);
	tag = &(bc->spares[page].tag);

	if(tag->dirty == TAG_CLEAR &&
		tag->valid == TAG_INVALID
#if defined(ENABLE_TAG_CHECKSUM) && ENABLE_TAG_CHECKSUM == 1
		&& tag->checkSum == 0xff
#endif
		) {
		return U_TRUE;
	}

	return U_FALSE;
}

/** 
 * \brief Is this block the last block of file ? (no free pages, and full filled with full pageID)
 */
UBOOL uffs_IsDataBlockReguFull(uffs_Device *dev, uffs_blockInfo *bc)
{
	uffs_LoadBlockInfo(dev, bc, dev->attr->pages_per_block - 1);
	if(bc->spares[dev->attr->pages_per_block - 1].tag.pageID == (dev->attr->pages_per_block - 1) &&
		bc->spares[dev->attr->pages_per_block - 1].tag.dataLength == dev->com.pgDataSize) {
		return U_TRUE;
	}
	return U_FALSE;
}

/** 
 * get partition used (bytes)
 */
int uffs_GetDeviceUsed(uffs_Device *dev)
{
	return (dev->par.end - dev->par.start + 1 - dev->tree.badCount
				- dev->tree.erasedCount) * dev->attr->block_data_size;
}

/** 
 * get partition free (bytes)
 */
int uffs_GetDeviceFree(uffs_Device *dev)
{
	return dev->tree.erasedCount * dev->attr->block_data_size;
}

/** 
 * get partition total size (bytes)
 */
int uffs_GetDeviceTotal(uffs_Device *dev)
{
	return (dev->par.end - dev->par.start + 1) * dev->attr->block_data_size;
}

/** \brief transfer the standard uffs_Tags to uffs_Tags_8
 *  \param[in] tag standard uffs_Tags
 *  \param[out] tag_8 small tag to fit into 8 bytes spare space
 */
void uffs_TransferToTag8(uffs_Tags *tag, uffs_Tags_8 *tag_8)
{
	tag_8->dirty = tag->dirty;
	tag_8->valid = tag->valid;
	tag_8->type = tag->type;
	tag_8->blockTimeStamp = tag->blockTimeStamp;
	tag_8->pageID = tag->pageID;
	tag_8->father = tag->father & 0xFF;
	tag_8->serial = tag->serial & 0xFF;
	tag_8->dataLength = tag->dataLength & 0xFF;
	tag_8->dataSum = tag->dataSum;
	tag_8->blockStatus = tag->blockStatus;
}

/** \brief transfer the small uffs_Tags_8 to standard uffs_Tags
 *  \param[out] tag standard uffs_Tags
 *  \param[in] tag_8 small tag to fit into 8 bytes spare space
 */
void uffs_TransferFromTag8(uffs_Tags *tag, uffs_Tags_8 *tag_8)
{
	tag->dirty = tag_8->dirty;
	tag->valid = tag_8->valid;
	tag->type = tag_8->type;
	tag->blockTimeStamp = tag_8->blockTimeStamp;
	tag->pageID = tag_8->pageID;
	tag->father = tag_8->father;
	tag->serial = tag_8->serial;
	tag->dataLength = tag_8->dataLength;
	tag->dataSum = tag_8->dataSum;
	tag->blockStatus = tag_8->blockStatus;
}

/***************************************

	Handle based memory manager

	Copyright 1995-2014 by Rebecca Ann Heineman becky@burgerbecky.com

	It is released under an MIT Open Source license. Please see LICENSE
	for license details. Yes, you can use it in a
	commercial title without paying anything, just give me a credit.
	Please? It's not like I'm asking you for money!

***************************************/

#include "brmemoryhandle.h"
#include "brdebug.h"
#include "brstringfunctions.h"
#include "brglobalmemorymanager.h"

/*! ************************************

	\class Burger::MemoryManagerHandle
	\brief Handle based Memory Manager
	
	This class allocates and releases memory using movable
	memory blocks and can allocate from the top and bottom of memory
	if needed. Fixed memory blocks are allocate from the 
	top of memory and movable memory blocks are allocated
	from the bottom. Movable blocks can be marked as purgable
	so in low memory situations, the memory can be freed without
	the main application's knowledge. To accomplish this, any
	access to a handle must be first locked and then tested
	if it's been purged. If it's purged, the memory must be reallocated
	and reloaded with the data. It's mostly used by the resource, texture
	and audio managers to cache in data chunks that can be
	reloaded from disk if need be.

***************************************/


/*! ************************************

	\struct Burger::MemoryManagerHandle::Handle_t
	\brief Structure describing an allocated chunk of memory
	
	This opaque structure contains all of the information that
	describes an allocated chunk of memory. The contents of this
	class is NEVER to be read or written to without the use
	of a MemoryManagerHandle call. The only exception is
	the first entry of m_pData which allows the structure
	to be used as a void ** to the data for instant access
	to the data.
	
	\note The data pointer can be \ref NULL of the memory was
	zero bytes in length or if the data was purged in an attempt 
	to free memory for an allocation in a low memory situation
	

***************************************/






/*! ************************************

	\brief Allocate fixed memory
	
	Static function to allocate a pointer to a block of memory in high (Fixed)
	memory.
	
	\param pThis Pointer to the MemoryManagerHandle instance
	\param uSize Size of memory block request
	\return Pointer to allocated memory block or \ref NULL on failure or zero byte allocation.
	\sa Burger::MemoryManagerHandle::FreeProc(MemoryManager *,const void *)

***************************************/

void *BURGER_API Burger::MemoryManagerHandle::AllocProc(MemoryManager *pThis,WordPtr uSize)
{
	if (uSize) {
		MemoryManagerHandle *pSelf = static_cast<MemoryManagerHandle *>(pThis);
		// Allocate the memory with memory for a back pointer
		void **ppData = pSelf->AllocHandle(uSize+ALIGNMENT,FIXED);
		// Got the memory?
		if (ppData) {
			// Dereference the memory!
			void *pData = reinterpret_cast<Handle_t*>(ppData)->m_pData;
			// Save the handle in memory
			static_cast<void ***>(pData)[0] = ppData;
			// Return the memory pointer at the next alignment value
	 		return static_cast<Word8 *>(pData)+ALIGNMENT;
		}
	}
	// Allocation failure
	return NULL;
}

/*! ************************************

	\brief Release fixed memory
	
	When a pointer is allocated using Burger::MemoryManagerHandle::AllocProc()
	It has a pointer to the handle that references this memory prepended to it.
	If the input is not \ref NULL it will use this prepended pointer to 
	release the handle and therefore this memory.
	
	\param pThis Pointer to the MemoryManagerHandle instance
	\param pInput Pointer to memory to release, \ref NULL does nothing
	\sa Burger::MemoryManagerHandle::AllocProc(MemoryManager *,WordPtr)

***************************************/

void BURGER_API Burger::MemoryManagerHandle::FreeProc(MemoryManager *pThis,const void *pInput)
{
	// Do nothing with NULL pointers
	if (pInput) {
		MemoryManagerHandle *pSelf = static_cast<MemoryManagerHandle *>(pThis);
		pSelf->FreeHandle(reinterpret_cast<void ** const *>(static_cast<const Word8 *>(pInput)-ALIGNMENT)[0]);
	}
}

/*! ************************************

	\brief Resize a preexisting allocated block of memory

	Using a pointer to memory, reallocate the size and copy
	the contents.
	If a zero length buffer is requested, the input pointer is deallocated,
	if the input pointer is \ref NULL, a fresh pointer is created.

	\param pThis Pointer to the MemoryManagerHandle instance
	\param pInput Pointer to memory to resize, \ref NULL forces a new block to be created
	\param uSize Size of memory block request
	\return Pointer to the new memory block or \ref NULL on failure.
	\sa Burger::MemoryManagerHandle::FreeProc(MemoryManager *,const void *)
	
***************************************/

void *BURGER_API Burger::MemoryManagerHandle::ReallocProc(Burger::MemoryManager *pThis,const void *pInput,WordPtr uSize)
{
	MemoryManagerHandle *pSelf = static_cast<MemoryManagerHandle *>(pThis);

	// Handle the two edge cases first

	// No input pointer?
	if (!pInput) {
		// Do I want any memory?
		if (uSize) {
			// Just get fresh memory
			pInput = AllocProc(pSelf,uSize);
		}

	// No memory requested?
	} else if (!uSize) {
		// Release the memory
		FreeProc(pSelf,pInput);
		// Return a null pointer
		pInput = NULL;
	} else {
	
		// Convert the pointer back into a handle and perform the resize operation

		void **ppData = pSelf->ReallocHandle(reinterpret_cast<void ** const *>(static_cast<const Word8 *>(pInput)-ALIGNMENT)[0],uSize+ALIGNMENT);
		// Successful?
		if (!ppData) {
			pInput = NULL;
		} else {
			// Dereference the memory!
			void *pData = reinterpret_cast<Handle_t*>(ppData)->m_pData;
			// Save the handle in memory
			static_cast<void ***>(pData)[0] = ppData;
			// Return the memory pointer at the next alignment value
 			pInput = static_cast<Word8 *>(pData)+ALIGNMENT;
		}
	}
	// Couldn't get the memory
	return const_cast<void *>(pInput);
}

/*! ************************************

	\brief Shutdown the handle based Memory Manager
	\param pThis Pointer to the MemoryManagerHandle instance
	\sa Burger::MemoryManagerHandle::Shutdown(void)

***************************************/

void BURGER_API Burger::MemoryManagerHandle::ShutdownProc(MemoryManager *pThis)
{
	MemoryManagerHandle *pSelf = static_cast<MemoryManagerHandle *>(pThis);

	// For debugging, test if all the memory is already released.
	// If not, report it

	// The final step is to release all of the memory 
	// allocated from the operating system

	// Get the first buffer
	SystemBlock_t *pBlock = pSelf->m_pSystemMemoryBlocks;
	if (pBlock) {
		SystemBlock_t *pNext;
		do {
			// Get the pointer to the next block
			pNext = pBlock->m_pNext;
			FreeSystemMemory(pBlock);
			pBlock = pNext;
		} while (pNext);
		pSelf->m_pSystemMemoryBlocks = NULL;
	}
	pSelf->m_pFreeHandle = NULL;
	pSelf->m_MemPurgeCallBack = NULL;
}


/*! ************************************

	\brief Allocate a new handle record.
	
	If out of handles in the pool,
	allocate memory from the operating system in \ref DEFAULTHANDLECOUNT * sizeof(\ref Handle_t) chunks
	\return Pointer to a new uninitialized Handle_t structure
	\sa Burger::MemoryManagerHandle::FreeHandle(void **)

***************************************/

Burger::MemoryManagerHandle::Handle_t * BURGER_API Burger::MemoryManagerHandle::AllocNewHandle(void)
{
	// Get a new handle
	Handle_t *pHandle = m_pFreeHandle;
	if (!pHandle) {
		// Get memory from system to prevent fragmentation
		WordPtr uChunkSize = ((DEFAULTHANDLECOUNT*sizeof(Handle_t))+sizeof(SystemBlock_t));
		SystemBlock_t *pBlock = static_cast<SystemBlock_t *>(AllocSystemMemory(uChunkSize));
		if (pBlock) {
			// Log the memory allocation
			m_uTotalSystemMemory += uChunkSize;
			m_uTotalHandleCount += DEFAULTHANDLECOUNT;

			// Mark this block for release on shutdown
			pBlock->m_pNext=m_pSystemMemoryBlocks;

			// Store the new master pointer
			m_pSystemMemoryBlocks = pBlock;

			// Adjust past the pointer
			pHandle = reinterpret_cast<Handle_t *>(pBlock+1);

			// Add the new handles to the free handle list
			Word i = DEFAULTHANDLECOUNT-1;								
			Handle_t *pNext = NULL;
			// Index to the last one
			pHandle = pHandle+(DEFAULTHANDLECOUNT-1);
			do {
				// Bad ID
				pHandle->m_uFlags = 0;
				pHandle->m_uID = MEMORYIDUNUSED;
				pHandle->m_pNextHandle = pNext;			// Link in the list
				pNext = pHandle;						// New parent
				--pHandle;								// Next handle
			} while (--i);								// All done?
			m_pFreeHandle = pNext;						// Save the new free handle list
			// pHandle has the first entry and it's not linked in. It's ready for use
		} else {
			// Non recoverable error!!
			Debug::Fatal("Out of system memory for handles!\n");
		}
	} else {
		// Unlink and continue
		m_pFreeHandle = pHandle->m_pNextHandle;
	}
	return pHandle;
}

/*! ************************************

	\brief Remove a range of memory from the free memory pool
	
	Assume that the memory range is either attached to the
	end of a free memory segment or the end of a free memory
	segment.

	If not, bad things will happen!

	\param pData Pointer to the start of the memory block to reserve
	\param uLength Size in bytes of the memory block to reserve
	\param pParent Pointer to the handle to the allocated handle BEFORE this block
	\param pHandle Handle of the currently free block of memory to allocate from or \ref NULL if the function has to scan manually
	\sa Burger::MemoryManagerHandle::ReleaseMemoryRange(void *,WordPtr,Handle_t *)
	
***************************************/

void BURGER_API Burger::MemoryManagerHandle::GrabMemoryRange(void *pData,WordPtr uLength,Handle_t *pParent,Handle_t *pHandle)
{
	// Pad the request to alignment size to ensure all blocks are aligned

	uLength = (uLength+(ALIGNMENT-1))&(~(ALIGNMENT-1));

	// Has the allocation block already been found?
	if (!pHandle) {

		// Manually beging the scan
		pHandle = m_FreeMemoryChunks.m_pNextHandle;

		// I will now scan free memory until I find the free memory
		// handle that contains the memory to be reserved

		for (;;) {
			// After free memory?
 			Word8 *pChunk = static_cast<Word8 *>(pHandle->m_pData);
			if (static_cast<Word8 *>(pData)>=pChunk) {
				if (static_cast<Word8 *>(pData)<(pChunk+pHandle->m_uLength)) {
					// pHandle has the memory block! 
					break;
				}
			}
			pHandle = pHandle->m_pNextHandle;
			if (pHandle==&m_FreeMemoryChunks) {
				// Only possible on data corruption
				DumpHandles();
				Debug::Fatal("Requested memory range to free is not in the free list\n");
				return;
			}
		}
	}

	// pHandle points to the block to obtain memory from.

	// Let's mark the parent entry
	pHandle->m_pNextPurge = pParent;
	// Allocated from the end of the data?
	if (pHandle->m_pData == pData) {
		// Full match?
		if (pHandle->m_uLength==uLength) {
			// Since I allocated the entire block, dispose of this
			// Unlink the free memory chunk
			Handle_t *pPrev = pHandle->m_pPrevHandle;
			Handle_t *pNext = pHandle->m_pNextHandle;
			pNext->m_pPrevHandle = pPrev;
			pPrev->m_pNextHandle = pNext;
			// Add to the free list
			// Mark the ID
			pHandle->m_uFlags = 0;
			pHandle->m_uID = MEMORYIDUNUSED;
			// Link in the list
			pHandle->m_pNextHandle = m_pFreeHandle;
			m_pFreeHandle = pHandle;
			return;
		}
		// Calc new length
		pHandle->m_uLength = pHandle->m_uLength-uLength;
		// Calc new beginning
		pHandle->m_pData = static_cast<Word8 *>(pData)+uLength;
		return;
	}

	// Memory is from end to the beginning
	// New length
	pHandle->m_uLength = static_cast<WordPtr>(static_cast<Word8 *>(pData)-static_cast<Word8 *>(pHandle->m_pData));
}

/*! ************************************

	\brief Add a range of memory to the free memory list.
	
	\note If this memory "Touches" another free memory entry, they will
	be merged together.

	\param pData Pointer to the memory block to return to the pool
	\param uLength Size of the memory block to return to the pool
	\param pParent Pointer to the Handle_t structure of the memory that was released
	\sa GrabMemoryRange(void *,WordPtr,Handle_t *,Handle_t *)

***************************************/

void BURGER_API Burger::MemoryManagerHandle::ReleaseMemoryRange(void *pData,WordPtr uLength,Handle_t *pParent)
{
	// Pad to nearest alignment
	uLength = (uLength+(ALIGNMENT-1))&(~(ALIGNMENT-1));
	Handle_t *pFreeChunk = &m_FreeMemoryChunks;
	Handle_t *pPrev = pFreeChunk->m_pPrevHandle;
	// No handles in the list?
	if (pPrev!=pFreeChunk) {
		// I will now scan free memory list until I find the free memory
		// handle after the memory to be freed

		do {
			// After free memory?
			if (static_cast<Word8 *>(pData)>=static_cast<Word8 *>(pPrev->m_pData)) {
				break;		// pFreeChunk = handle
			}
			pPrev = pPrev->m_pPrevHandle;
		} while (pPrev!=pFreeChunk);

		// pFreeChunk has the free memory handle AFTER the memory

		pFreeChunk = pPrev->m_pNextHandle;

		// See if this free memory is just an extension of the previous

		// Pointer to the end of the free memory 
		Word8 *pEnd = static_cast<Word8 *>(pPrev->m_pData)+pPrev->m_uLength;

		// Does the end match this block?
		if (pEnd==static_cast<Word8 *>(pData)) {
			// Set the new parent handle
			pPrev->m_pNextPurge = pParent;
			// Extend this block to add this memory
			pPrev->m_uLength = pPrev->m_uLength+uLength;
			// Last handle?
			if (pFreeChunk!=&m_FreeMemoryChunks) {
				// Was this the case where a hole between two entries merge?
				pEnd = pEnd+uLength;
				// Filled in two free mems?
				if (pEnd==static_cast<Word8 *>(pFreeChunk->m_pData)) {
					// Extend again!
					pPrev->m_uLength = pPrev->m_uLength+pFreeChunk->m_uLength;
					// Remove the second handle
					pPrev->m_pNextHandle = pFreeChunk->m_pNextHandle;
					pFreeChunk->m_pNextHandle->m_pPrevHandle = pPrev;

					// Release this handle to the free pool
					pFreeChunk->m_uFlags = 0;		// Bad ID
					pFreeChunk->m_uID = MEMORYIDUNUSED;
					pFreeChunk->m_pNextHandle = m_pFreeHandle;	// Link in the list
					m_pFreeHandle = pFreeChunk;					// New parent
				}
			}
		} else {

			// Check If I should merge with the next fragment

			// Get the current memory fragment
			pEnd = static_cast<Word8 *>(pData)+uLength;
			// Does it touch next handle?
			if (pEnd==static_cast<Word8 *>(pFreeChunk->m_pData)) {
				pFreeChunk->m_pNextPurge = pParent;	// New parent
				pFreeChunk->m_uLength = pFreeChunk->m_uLength+uLength;		// New length
				pFreeChunk->m_pData = pData;								// New start pointer
			} else {

				// It is not mergable... I need to create a handle

				Handle_t *pNew = AllocNewHandle();		// Get a new handle
				pNew->m_pData = pData;					// New pointer
				pNew->m_uFlags = 0;
				pNew->m_uID = MEMORYIDFREE;
				pNew->m_uLength = uLength;				// Free mem length
				pNew->m_pNextHandle = pFreeChunk;		// Forward handle
				pNew->m_pPrevHandle = pPrev;			// Previous handle
				pNew->m_pNextPurge = pParent;			// Set the new parent
				pNew->m_pPrevPurge = NULL;
				// Link me in
				pPrev->m_pNextHandle = pNew;
				pFreeChunk->m_pPrevHandle = pNew;
			}
		}
	} else {

		// There is no free memory, create the singular entry

		pPrev = AllocNewHandle();
		// Create the free memory entry
		pPrev->m_pData = pData;
		pPrev->m_uLength = uLength;
		// Mark the parent handle
		pPrev->m_uFlags = 0;
		pPrev->m_uID = MEMORYIDFREE;
		pPrev->m_pNextHandle = pFreeChunk;
		pPrev->m_pPrevHandle = pFreeChunk;
		pPrev->m_pNextPurge = pParent;
		pPrev->m_pPrevPurge = NULL;
		// Link the new entry to the free list
		pFreeChunk->m_pNextHandle = pPrev;
		pFreeChunk->m_pPrevHandle = pPrev;
	}
}

/*! ************************************

	\brief Print the state of the memory to Burger::Debug::String(const char *)

	Walk the linked list of handles from pFirst to pLast and print
	to the text output a report of the memory handles. All text is printed via Debug::String(const char *)

	\param pFirst Pointer to the first block to dump
	\param pLast Pointer to the block that the dump will cease (Don't dump it)
	\param bNoCheck If true, print pFirst at all times
	\sa Debug::String(const char *)
	
***************************************/

void BURGER_API Burger::MemoryManagerHandle::PrintHandles(const Handle_t *pFirst,const Handle_t *pLast,Word bNoCheck)
{
	char FooBar[256];

	// Init handle count
	Word uCount = 1;
	
#if BURGER_MAXWORDPTR==0xFFFFFFFFU
	Debug::String("#     Handle    Addr   Attr  ID    Size     Prev     Next\n");
//				  "0000 00000000 00000000 0000 0000 00000000 00000000 00000000"
	if (bNoCheck || pFirst!=pLast) {
		do {
			NumberToAsciiHex(&FooBar[0],static_cast<Word32>(uCount),NOENDINGNULL|LEADINGZEROS|4);
			FooBar[4] = ' ';
			NumberToAsciiHex(&FooBar[5],static_cast<Word32>(reinterpret_cast<WordPtr>(pFirst)),NOENDINGNULL|LEADINGZEROS|8);
			FooBar[13] = ' ';
			NumberToAsciiHex(&FooBar[14],static_cast<Word32>(reinterpret_cast<WordPtr>(pFirst->m_pData)),NOENDINGNULL|LEADINGZEROS|8);
			FooBar[22] = ' ';
			NumberToAsciiHex(&FooBar[23],static_cast<Word32>(pFirst->m_uFlags),NOENDINGNULL|LEADINGZEROS|4);
			FooBar[27] = ' ';
			NumberToAsciiHex(&FooBar[28],static_cast<Word32>(pFirst->m_uID),NOENDINGNULL|LEADINGZEROS|4);
			FooBar[32] = ' ';
			NumberToAsciiHex(&FooBar[33],static_cast<Word32>(pFirst->m_uLength),NOENDINGNULL|LEADINGZEROS|8);
			FooBar[41] = ' ';
			NumberToAsciiHex(&FooBar[42],static_cast<Word32>(reinterpret_cast<WordPtr>(pFirst->m_pPrevHandle)),NOENDINGNULL|LEADINGZEROS|8);
			FooBar[50] = ' ';
			NumberToAsciiHex(&FooBar[51],static_cast<Word32>(reinterpret_cast<WordPtr>(pFirst->m_pNextHandle)),NOENDINGNULL|LEADINGZEROS|8);
			FooBar[59] = '\n';
			FooBar[60] = 0;
			Debug::String(FooBar);
			// Next handle in chain
			pFirst = pFirst->m_pNextHandle;
			++uCount;
			// All done?
		} while (pFirst!=pLast);
	}
#else
	Debug::String("#         Handle            Addr       Attr  ID        Size             Prev             Next\n");
//				  "0000 0000000000000000 0000000000000000 0000 0000 0000000000000000 0000000000000000 0000000000000000"
	if (bNoCheck || pFirst!=pLast) {
		do {
			NumberToAsciiHex(&FooBar[0],static_cast<Word32>(uCount),NOENDINGNULL|LEADINGZEROS|4);
			FooBar[4] = ' ';
			NumberToAsciiHex(&FooBar[5],reinterpret_cast<WordPtr>(pFirst),NOENDINGNULL|LEADINGZEROS|16);
			FooBar[21] = ' ';
			NumberToAsciiHex(&FooBar[22],reinterpret_cast<WordPtr>(pFirst->m_pData),NOENDINGNULL|LEADINGZEROS|16);
			FooBar[38] = ' ';
			NumberToAsciiHex(&FooBar[39],static_cast<Word32>(pFirst->m_uFlags),NOENDINGNULL|LEADINGZEROS|4);
			FooBar[43] = ' ';
			NumberToAsciiHex(&FooBar[44],static_cast<Word32>(pFirst->m_uID),NOENDINGNULL|LEADINGZEROS|4);
			FooBar[48] = ' ';
			NumberToAsciiHex(&FooBar[49],pFirst->m_uLength,NOENDINGNULL|LEADINGZEROS|16);
			FooBar[65] = ' ';
			NumberToAsciiHex(&FooBar[66],reinterpret_cast<WordPtr>(pFirst->m_pPrevHandle),NOENDINGNULL|LEADINGZEROS|16);
			FooBar[82] = ' ';
			NumberToAsciiHex(&FooBar[83],reinterpret_cast<WordPtr>(pFirst->m_pNextHandle),NOENDINGNULL|LEADINGZEROS|16);
			FooBar[99] = '\n';
			FooBar[100] = 0;
			Debug::String(FooBar);
			// Next handle in chain
			pFirst = pFirst->m_pNextHandle;
			++uCount;
			// All done?
		} while (pFirst!=pLast);
	}
#endif
}




/***************************************

	Public functions

***************************************/




/*! ************************************

	\brief Initialize the Handle based Memory Manager
	
	\note If this class cannot start up due to memory starvation, it will fail with 
	a call to Debug::Fatal()
	
	\sa Burger::MemoryManagerHandle::~MemoryManagerHandle()

***************************************/

Burger::MemoryManagerHandle::MemoryManagerHandle(WordPtr uDefaultMemorySize,Word uDefaultHandleCount,WordPtr uMinReserveSize)
{
	// No memory allocated yet
	m_uTotalAllocatedMemory = 0;
	m_uTotalSystemMemory = 0;
	m_uTotalHandleCount = 0;
	m_pSystemMemoryBlocks = NULL;

	// No memory callback
	m_MemPurgeCallBack = NULL;
	m_pMemPurge = NULL;

	// Init my global pointers
	m_pAlloc = AllocProc;
	m_pFree = FreeProc;
	m_pRealloc = ReallocProc;
	m_pShutdown = ShutdownProc;

	// At this point, if there was an initialization failure,
	// Shutdown() will clean up gracefully

	// Obtain the base memory from the operating system

	// Enough for the OS?
	void *pReserved = AllocSystemMemory(uMinReserveSize);
	if (!pReserved) {
		// You're boned
		Debug::Fatal("Can't allocate minimum OS memory chunk\n");
	}

	// Allocate the super chunk

	WordPtr uSwing = uDefaultMemorySize;
	// Try for the entire block
	SystemBlock_t *pBlock = static_cast<SystemBlock_t *>(AllocSystemMemory(uSwing));
	if (!pBlock) {
		// Low on memory, do a binary search to see how much is present
		// First bisection
		uSwing = uSwing>>1U;
		WordPtr uMinsize = 0;					// No minimum available found yet
		for (;;) {
			WordPtr uSize = uMinsize+uSwing;	// Attempt this size
			pBlock = static_cast<SystemBlock_t *>(AllocSystemMemory(uSize));	// Try to allocate it
			if (pBlock) {
				FreeSystemMemory(pBlock);
				uMinsize = uSize;				// This is acceptable!
			} else {
				uDefaultMemorySize = uSize;		// Can't get bigger than this!
			}
			uSwing = (uDefaultMemorySize-uMinsize)>>1U;	// Get half the differance
			if (uSwing<1024) {		// Close enough?
				pBlock = static_cast<SystemBlock_t *>(AllocSystemMemory(uMinsize));
				uSwing = uMinsize;
				break;
			}
		}
	}
	// This is my superblock
	m_pSystemMemoryBlocks = pBlock;
	m_uTotalSystemMemory = uSwing;
	
	// Release OS memory
	FreeSystemMemory(pReserved);

	// No memory at all???
	if (!pBlock) {
		// Die!
		Debug::Fatal("Can't allocate super chunk\n");
		return;
	}
	// Mark the next link so Shutdown works if fatal
	pBlock->m_pNext = NULL;

	// Not enough memory for any good 
	if (uSwing<0x10000) {
		Debug::Fatal("Super chunk is less than 64K bytes\n");
	}

	// Initialize the free handle list
	Handle_t *pHandle = reinterpret_cast<Handle_t *>(pBlock+1);
	// This is my remaining memory
	uSwing -= sizeof(SystemBlock_t);

	// Check for boneheads
	if (uDefaultHandleCount<8) {
		uDefaultHandleCount = 8;
	}

	m_uTotalHandleCount = uDefaultHandleCount;
	// Memory needed for handles
	WordPtr uHandleSize = uDefaultHandleCount*sizeof(Handle_t);
	// You've got to be kidding me!!
	if (uHandleSize>=uSwing) {	
		// Die!
		Debug::Fatal("Can't allocate default handle array\n");
	}

	// Link all the handles in the free handle list

	// Set the next pointer to NULL since it's the last link
	Handle_t *pNextHandle = NULL;
	// Start at the last entry
	pHandle = pHandle+(uDefaultHandleCount-1);
	do {
		// Iterate backwards
		pHandle->m_uFlags = 0;
		pHandle->m_uID = MEMORYIDUNUSED;		// Bad ID
		pHandle->m_pNextHandle = pNextHandle;	// Link in the list
		pNextHandle = pHandle;					// New parent
		--pHandle;
	} while (--uDefaultHandleCount);			// All done?
	// New free handle linked list head pointer
	m_pFreeHandle = pNextHandle;

	// "Use up" the memory for the handles
	pHandle = pHandle+(m_uTotalHandleCount+1);
	uSwing = uSwing-uHandleSize;

	// Align the pointer
	WordPtr uSuperChunkStart = (reinterpret_cast<WordPtr>(pHandle)+(ALIGNMENT-1)) & (~(ALIGNMENT-1));
	uSwing -= uSuperChunkStart-reinterpret_cast<WordPtr>(pHandle);

	// Initialize the Used handle list for free memory strips

	// Used memory starts at zero and ends at the free pointer
	m_LowestUsedMemory.m_pData = NULL;
	m_LowestUsedMemory.m_uLength = uSuperChunkStart;
	m_LowestUsedMemory.m_uFlags = LOCKED|FIXED;
	m_LowestUsedMemory.m_uID = MEMORYIDRESERVED;
	m_LowestUsedMemory.m_pNextHandle = &m_HighestUsedMemory;
	m_LowestUsedMemory.m_pPrevHandle = &m_HighestUsedMemory;
	m_LowestUsedMemory.m_pNextPurge = NULL;
	m_LowestUsedMemory.m_pPrevPurge = NULL;

	// Used memory continues from the end of the free buffer to the end of memory
	Word8 *pEnd = reinterpret_cast<Word8*>(uSuperChunkStart)+uSwing;
	m_HighestUsedMemory.m_pData = pEnd;
	m_HighestUsedMemory.m_uLength = static_cast<WordPtr>(-1) - reinterpret_cast<WordPtr>(pEnd);
	m_HighestUsedMemory.m_uFlags = LOCKED|FIXED;
	m_HighestUsedMemory.m_uID = MEMORYIDRESERVED;
	m_HighestUsedMemory.m_pNextHandle = &m_LowestUsedMemory;
	m_HighestUsedMemory.m_pPrevHandle = &m_LowestUsedMemory;
	m_HighestUsedMemory.m_pNextPurge = NULL;
	m_HighestUsedMemory.m_pPrevPurge = NULL;

	// Initialize the list of handles that have free memory blocks
	m_FreeMemoryChunks.m_pData = NULL;
	m_FreeMemoryChunks.m_uLength = 0;
	m_FreeMemoryChunks.m_uFlags = 0;
	m_FreeMemoryChunks.m_uID = MEMORYIDRESERVED;
	m_FreeMemoryChunks.m_pNextHandle = &m_FreeMemoryChunks;
	m_FreeMemoryChunks.m_pPrevHandle = &m_FreeMemoryChunks;
	m_FreeMemoryChunks.m_pNextPurge = NULL;
	m_FreeMemoryChunks.m_pPrevPurge = NULL;

	m_PurgeHands.m_pData = NULL;
	m_PurgeHands.m_uLength = 0;
	m_PurgeHands.m_uFlags = 0;
	m_PurgeHands.m_uID = MEMORYIDRESERVED;
	m_PurgeHands.m_pNextHandle = &m_PurgeHands;
	m_PurgeHands.m_pPrevHandle = &m_PurgeHands;
	m_PurgeHands.m_pNextPurge = NULL;
	m_PurgeHands.m_pPrevPurge = NULL;

	m_PurgeHandleFiFo.m_pData = NULL;
	m_PurgeHandleFiFo.m_uLength = 0;
	m_PurgeHandleFiFo.m_uFlags = 0;
	m_PurgeHandleFiFo.m_uID = MEMORYIDRESERVED;
	m_PurgeHandleFiFo.m_pNextHandle = &m_PurgeHandleFiFo;
	m_PurgeHandleFiFo.m_pPrevHandle = &m_PurgeHandleFiFo;
	m_PurgeHandleFiFo.m_pNextPurge = &m_PurgeHandleFiFo;
	m_PurgeHandleFiFo.m_pPrevPurge = &m_PurgeHandleFiFo;

	// Create the default free list
	ReleaseMemoryRange(reinterpret_cast<void *>(uSuperChunkStart),uSwing,&m_LowestUsedMemory);
}

/*! ************************************

	\brief The destructor for the Handle based Memory Manager

	This calls Burger::MemoryManagerHandle::Shutdown(MemoryManager *) to do the actual work

	\sa Shutdown(MemoryManager *)

***************************************/

Burger::MemoryManagerHandle::~MemoryManagerHandle()
{
	ShutdownProc(this);
}

/*! ************************************

	\fn Burger::MemoryManagerHandle::GetTotalAllocatedMemory(void) const
	\brief Returns the total allocated memory used by pointers and handles in bytes.

	This is the total number of bytes allocated with all padding necessary for data alignment
	
	\sa Burger::MemoryManagerHandle::GetTotalFreeMemory(void)
	
***************************************/

/*! ************************************

	\fn Burger::MemoryManagerHandle::Alloc(WordPtr)
	\brief Allocate fixed memory. 

	Allocates a pointer to a block of memory in high (Fixed) memory.
	
	\param uSize Number of bytes requested 
	\return \ref NULL on allocation failure, valid handle to memory if successful
	\sa Burger::MemoryManagerHandle::AllocProc(MemoryManager *,WordPtr)
	
***************************************/

/*! ************************************

	\fn Burger::MemoryManagerHandle::Free(const void *)
	\brief Release fixed memory.

	When a pointer is allocated using Burger::MemoryManagerHandle::Alloc(WordPtr),
	it has a pointer to the handle that references this memory prepended to it.
	If the input is not \ref NULL it will use this prepended pointer to
	release the handle and therefore this memory.
	
	\param pInput Pointer to memory to release, \ref NULL does nothing 
	\sa Burger::MemoryManagerHandle::FreeProc(MemoryManager *,const void *)
	
***************************************/

/*! ************************************

	\fn Burger::MemoryManagerHandle::Realloc(const void *,WordPtr)
	\brief Resize a preexisting allocated block of memory.   

	Using a pointer to memory, reallocate the size and copy the contents.
	If a zero length buffer is requested, the input pointer is deallocated,
	if the input pointer is \ref NULL, a fresh pointer is created.
	
	\param pInput Pointer to memory to resize, \ref NULL forces a new block to be created
	\param uSize Size of memory block request
	\return Pointer to the new memory block or \ref NULL on failure.
	\sa Burger::MemoryManagerHandle::ReallocProc(MemoryManager *,const void *,WordPtr)
	
***************************************/

/*! ************************************

	\fn Burger::MemoryManagerHandle::Shutdown(void)
	\brief Shutdown the handle based Memory Manager. 

	\sa Burger::MemoryManagerHandle::ShutdownProc(MemoryManager *)
	
***************************************/


/*! ************************************

	\brief Allocates a block of memory

	Allocates from the top down if fixed and bottom up if movable
	This routine handles all the magic for memory purging and
	allocation.

	\param uSize Number of bytes requested
	\param uFlags Flags to modify how the memory is allocated, \ref FIXED for fixed memory, 0 for movable memory
	
	\return \ref NULL on allocation failure, valid handle to memory if successful
	\sa Burger::MemoryManagerHandle::FreeHandle(void **)
	
***************************************/

void ** BURGER_API Burger::MemoryManagerHandle::AllocHandle(WordPtr uSize,Word uFlags)
{
	// Don't allocate an empty handle!
	if (!uSize) {
		return NULL;
	}	
	// Initialized?
	if (m_pSystemMemoryBlocks) {
		// Get a new handle
		Handle_t *pNew = AllocNewHandle();
		if (!pNew) {
			return NULL;
		}
		pNew->m_pNextPurge = NULL;
		pNew->m_pPrevPurge = NULL;
		// Save the handle size WITHOUT padding
		pNew->m_uLength = uSize;
		// Save the default attributes
		pNew->m_uFlags = uFlags&(~MALLOC);
		// Init data memory search stage
		eMemoryStage eStage = StageCompact;
		// Round up
		uSize = (uSize+(ALIGNMENT-1)) & (~(ALIGNMENT-1));

		if (uFlags&FIXED) {

			// Scan from the top down for fixed handles
			// Increases odds for compaction success

			for (;;) {
				Handle_t *pEntry = m_FreeMemoryChunks.m_pPrevHandle;

				// Find the memory, pNew has the handle the memory
				// will occupy BEFORE, pEntry is the prev handle before the new one

				// No free memory?
				if (pEntry != &m_FreeMemoryChunks) {
					do {
						if (pEntry->m_uLength>=uSize) {
							Handle_t *pPrev = pEntry->m_pNextPurge;	// Get the parent handle
							Handle_t *pNext = pPrev->m_pNextHandle;	// Next handle
							
							pNew->m_pPrevHandle = pPrev;
							pNew->m_pNextHandle = pNext;
							pPrev->m_pNextHandle = pNew;
							pNext->m_pPrevHandle = pNew;

							void *pData = (static_cast<Word8 *>(pEntry->m_pData)+(pEntry->m_uLength-uSize));
							pNew->m_pData = pData;
							GrabMemoryRange(pData,uSize,pPrev,pEntry);
								
							// Update the global allocated memory count.
							m_uTotalAllocatedMemory += pNew->m_uLength;
							// Good allocation!
							return reinterpret_cast<void **>(pNew);
						}
						// Look at next handle
						pEntry=pEntry->m_pPrevHandle;
						// End of the list?
					} while (pEntry != &m_FreeMemoryChunks);
				}
				if (eStage==StageCompact) {
					// Pack memory together
					CompactHandles();
					eStage=StagePurge;
				} else if (eStage==StagePurge) {
					// Purge the handles
					if (PurgeHandles(uSize)) {
						// Try again with compaction
						eStage=StageCompact;
					} else {
						// This is where giving up is the right thing
						eStage = StageHailMary;
					}
				} else if (eStage==StageHailMary) {
					break;
				}
			}

		} else {
			// Scan from the bottom up for movable handles
			// Increases odds for compaction success

			for (;;) {
				// Get next index
				Handle_t *pEntry = m_FreeMemoryChunks.m_pNextHandle;

				// Find the memory, pNew has the handle the memory
				// will occupy AFTER, pEntry is the next handle after the new one

				// Already stop?
				if (pEntry != &m_FreeMemoryChunks) {
					do {
						if (pEntry->m_uLength>=uSize) {
							// Get the parent handle
							Handle_t *pPrev = pEntry->m_pNextPurge;
							Handle_t *pNext = pPrev->m_pNextHandle;

							pNew->m_pPrevHandle = pPrev;
							pNew->m_pNextHandle = pNext;
							pPrev->m_pNextHandle = pNew;
							pNext->m_pPrevHandle = pNew;

							pNew->m_pData = pEntry->m_pData;
							GrabMemoryRange(pEntry->m_pData,uSize,pNew,pEntry);
								
							// Update the global allocated memory count.
							m_uTotalAllocatedMemory += pNew->m_uLength;
							// Good allocation!
							return reinterpret_cast<void **>(pNew);
						}
						// Look at next handle
						pEntry=pEntry->m_pNextHandle;
						// End of the list?
					} while (pEntry != &m_FreeMemoryChunks);
				}
				if (eStage==StageCompact) {
					// Pack memory together
					CompactHandles();
					eStage=StagePurge;
				} else if (eStage==StagePurge) {
					// Purge the handles
					if (PurgeHandles(uSize)) {
						// Try again with compaction
						eStage=StageCompact;
					} else {
						// This is where giving up is the right thing
						eStage = StageHailMary;
					}
				} else if (eStage==StageHailMary) {
					break;
				}
			}
		}
		// Failed in the quest for memory, exit as a miserable loser

		// Restore the actual byte request (Not padded)
		uSize = pNew->m_uLength;
		// Bad ID
		pNew->m_uFlags = 0;
		pNew->m_uID = MEMORYIDUNUSED;
		// Link in the list
		pNew->m_pNextHandle = m_pFreeHandle;
		// New parent
		m_pFreeHandle = pNew;
	}
	// Try to get memory from somewhere else.
	// This is a last resort!
		
	Handle_t *pNewAlloc = static_cast<Handle_t *>(AllocSystemMemory(uSize+sizeof(Handle_t)+ALIGNMENT));
	if (!pNewAlloc) {
		// Give up
		return NULL;
	}
	// Update the global allocated memory count.
	m_uTotalAllocatedMemory += uSize;
		
	pNewAlloc->m_uLength = uSize;
	pNewAlloc->m_uFlags = uFlags|MALLOC;	// It was Malloc'd
	pNewAlloc->m_pPrevHandle = NULL;	// Force crash
	pNewAlloc->m_pNextHandle = NULL;
	pNewAlloc->m_pNextPurge = NULL;
	pNewAlloc->m_pPrevPurge = NULL;
	// Ensure data alignment
	pNewAlloc->m_pData = reinterpret_cast<void*>((reinterpret_cast<WordPtr>(pNewAlloc)+sizeof(Handle_t)+(ALIGNMENT-1)) & (~(ALIGNMENT-1)));
	// Return the fake handle
	return reinterpret_cast<void **>(pNewAlloc);
}

/*! ************************************

	\brief Dispose of a memory handle into the free handle pool

	\note \ref NULL is acceptable input.

	\param ppInput Handle to memory allocated by AllocHandle(WordPtr,Word)

***************************************/

void BURGER_API Burger::MemoryManagerHandle::FreeHandle(void **ppInput)
{
	// Valid handle?
	if (ppInput) {
		// Subtract from global size.
		Handle_t *pHandle = reinterpret_cast<Handle_t *>(ppInput);
		m_uTotalAllocatedMemory -= pHandle->m_uLength;
	
		if (!(pHandle->m_uFlags&MALLOC)) {
			// Only perform an action if the class
			// is initialized. Otherwise assume an out of order
			// class shutdown and do nothing

			if (m_pSystemMemoryBlocks) {
				Handle_t *pPrev;

				// If this handle is on the purge list,
				// unlink it
				Handle_t *pNext = pHandle->m_pNextPurge;
				if (pNext) {
					// Remove from the linked purge list
					pPrev = pHandle->m_pPrevPurge;
					pPrev->m_pNextPurge = pNext;
					pNext->m_pPrevPurge = pPrev;
				}

				// Unlink from the main list
				pNext = pHandle->m_pNextHandle;
				pPrev = pHandle->m_pPrevHandle;
				pPrev->m_pNextHandle = pNext;
				pNext->m_pPrevHandle = pPrev;
				
				// Release the memory range back into the pool
				// if there was any memory attached to this handle
				void *pData = pHandle->m_pData;
				if (pData) {
					ReleaseMemoryRange(pData,pHandle->m_uLength,pPrev);
				}
				// Add in this handle to the free list
				// Mark with INVALID entry ID
				pHandle->m_uFlags = 0;
				pHandle->m_uID = MEMORYIDUNUSED;
				pHandle->m_pNextHandle = m_pFreeHandle;
				m_pFreeHandle = pHandle;
			}
		} else {
			// Just release the memory
			FreeSystemMemory(pHandle);
		}
	}
}

/*! ************************************

	\brief Resize a handle

	Using a handle to memory, reallocate the size and copy
	the contents. If the input handle is \ref NULL, then just allocate a
	new handle, if the size requested is zero then discard the input
	handle.

	\param ppInput Handle to resize
	\param uSize Size in bytes of the new handle
	\return Handle of the newly resized memory chunk
	\sa Burger::MemoryManagerHandle::FreeHandle(void **) and Burger::MemoryManagerHandle::AllocHandle(WordPtr,Word)

***************************************/

void ** BURGER_API Burger::MemoryManagerHandle::ReallocHandle(void **ppInput,WordPtr uSize)
{
	// No previous handle?
	if (!ppInput) {
		// New memory requested?
		if (uSize) {
			// Allocate the new memory
			return AllocHandle(uSize,0);
		}
		return NULL;
	}

	// No memory requested?
	if (!uSize) {
		// Release the original
		FreeHandle(ppInput);
		// Normal exit
		return NULL;
	}

	// Try the easy way, just shrink the handle...
	Handle_t *pHandle = reinterpret_cast<Handle_t *>(ppInput);
	// Get length
	WordPtr uOldSize = pHandle->m_uLength;
	if (uSize==uOldSize) {
		// Return the handle without any changes
		return ppInput;
	}

	// Handle will shrink??
	if (uSize<uOldSize &&
		// Not manually allocated?
		(!(pHandle->m_uFlags & MALLOC))) {
		pHandle->m_uLength = uSize;		// Set the new size
		uSize = (uSize+(ALIGNMENT-1))&(~(ALIGNMENT-1));		// Long word align
		uOldSize = (uOldSize+(ALIGNMENT-1))&(~(ALIGNMENT-1));
		uOldSize = uOldSize-uSize;	// How many bytes to release?
		if (uOldSize) {				// Will I release any memory?
			Word8 *pStart = static_cast<Word8 *>(pHandle->m_pData)+uSize;	/* Get start pointer */
			ReleaseMemoryRange(pStart,uOldSize,pHandle);
		}
		return ppInput;		// Return the smaller handle
	}

	// Handle is growing...
	// I have to do it the hard way!!

	// Allocate the new memory
	Handle_t *pNew = reinterpret_cast<Handle_t *>(AllocHandle(uSize,pHandle->m_uFlags));
	if (pNew) {		// Success!
		if (uSize<uOldSize) {		// Make sure I only copy the SMALLER of the two
			uOldSize = uSize;		// New size
		}
		// Copy the contents
		MemoryCopy(pNew->m_pData,pHandle->m_pData,uOldSize);
	}
	// Release the previous memory
	FreeHandle(ppInput);
	// Return the new pointer
	return reinterpret_cast<void **>(pNew);	
}

/*! ************************************

	\brief If the handle was purged, reallocate memory to it.

	\note The returned handle will REPLACE the handle that was passed in.
	This code effectively disposes of the previous handle and allocates
	a new one of the old one's size. If the data is still intact then
	nothing happens
	
	\param ppInput Handle to be restored
	\return New handle for data

	\sa Burger::MemoryManagerHandle::SetPurgeFlag(void **,Word)
	
***************************************/

void ** BURGER_API Burger::MemoryManagerHandle::RefreshHandle(void **ppInput)
{
	if (ppInput) {
		if (*ppInput) {		// Handle already valid?
			SetPurgeFlag(ppInput,FALSE);			// Don't purge now
			return ppInput;	// Leave now!
		}
		// How much memory to allocate
		WordPtr uSize = reinterpret_cast<const Handle_t *>(ppInput)->m_uLength;
		Word uFlags = reinterpret_cast<const Handle_t *>(ppInput)->m_uFlags;
		FreeHandle(ppInput);		// Dispose of the old handle
		return AllocHandle(uSize,uFlags);	// Create a new one with the old size
	}
	return NULL;
}

/*! ************************************

	\brief Search the handle tree for a pointer
	
	\note The pointer does NOT have to be the head pointer, just in the domain of the handle
	Return \ref NULL if the handle is not here.

	\param pInput Pointer to memory to locate
	\return MemoryManagerHandle::Handle_t to handle that contains the pointer.
	\sa Burger::MemoryManagerHandle::GetSize(void **)
	
***************************************/

void ** BURGER_API Burger::MemoryManagerHandle::FindHandle(const void *pInput)
{
	// Get the first handle
	Handle_t *pHandle = m_LowestUsedMemory.m_pNextHandle;
	// Are there handles?
	if (pHandle!=&m_HighestUsedMemory) {
		do {
			// Get the handle's memory pointer
			Word8 *pData = static_cast<Word8 *>(pHandle->m_pData);
			// Is it too far? 
			if (pData>static_cast<const Word8 *>(pInput)) {
				// Abort now...
				break;
			}
			// Get the final byte address
			pData += pHandle->m_uLength;
			// In range?
			if (pData>static_cast<const Word8 *>(pInput)) {
				// This is the handle!
				return reinterpret_cast<void **>(pHandle);
			}
			pHandle = pHandle->m_pNextHandle;
			// List still valid?
		} while (pHandle!=&m_HighestUsedMemory);
	}
	// Didn't find it...
	return NULL;
}

/*! ************************************

	\brief Returns the size of a memory handle
	\param ppInput Valid handle of allocated memory, or \ref NULL
	\return Number of bytes handle controls or zero if empty or \ref NULL input
	\sa Burger::MemoryManagerHandle::GetSize(const void *)
	
***************************************/

WordPtr BURGER_API Burger::MemoryManagerHandle::GetSize(void **ppInput)
{
	if (ppInput) {
		return reinterpret_cast<Handle_t *>(ppInput)->m_uLength;
	}
	return 0;
}


/*! ************************************

	\brief Returns the size of a memory pointer
	\param pInput Valid pointer to allocated memory or \ref NULL
	\return Number of bytes pointer controls or zero if \ref NULL
	\sa Burger::MemoryManagerHandle::GetSize(void **)

***************************************/

WordPtr BURGER_API Burger::MemoryManagerHandle::GetSize(const void *pInput)
{
	if (pInput) {			// Null pointer?!?
		const Handle_t *pHandle = reinterpret_cast<const Handle_t * const *>(static_cast<const Word8 *>(pInput)-ALIGNMENT)[0];
		return pHandle->m_uLength;
	}
	return 0;
}


/*! ************************************

	\brief Returns the total free space with purging

	This is accomplished by adding all the memory found in
	the free memory linked list and then adding all the memory
	in the used list that can be purged.

	\return Number of bytes available for allocation including all purgable memory
	
***************************************/

WordPtr BURGER_API Burger::MemoryManagerHandle::GetTotalFreeMemory(void)
{
	WordPtr uFree=0;		// Running total

	// Add all the free memory handles

	Handle_t *pHandle = m_FreeMemoryChunks.m_pNextHandle;	// Follow the entire list
	if (pHandle!=&m_FreeMemoryChunks) {			// List valid?
		do {
			uFree += pHandle->m_uLength;		// Just add it in
			pHandle = pHandle->m_pNextHandle;	// Next one in chain
		} while (pHandle!=&m_FreeMemoryChunks);	// All done?
	}

	// Now traverse the used list for all purgable memory

	pHandle = m_LowestUsedMemory.m_pNextHandle;		// Find all purgable memory
	if (pHandle!=&m_HighestUsedMemory) {			// Valid chain?
		do {
			if (!(pHandle->m_uFlags&LOCKED) &&	// Unlocked and purgeable
				(pHandle->m_pNextPurge)) {
				// Round up the length
				WordPtr uTemp = (pHandle->m_uLength+(ALIGNMENT-1)) & (~(ALIGNMENT-1));
				uFree += uTemp;		/* Add into the total */
			}
			// Next link
			pHandle = pHandle->m_pNextHandle;
			// All done?
		} while (pHandle!=&m_HighestUsedMemory);
	}
	// Return the free size
	return uFree;
}

/*! ************************************

	\brief Set the lock flag to a given handle and return the data pointer
	
	\note This is a boolean flag, not reference counted
	
	\param ppInput Pointer to handle to lock or \ref NULL
	\return Pointer to dereferenced memory or \ref NULL on failure or \ref NULL input
	\sa Burger::MemoryManagerHandle::Unlock(void **ppInput)

***************************************/

void * BURGER_API Burger::MemoryManagerHandle::Lock(void **ppInput)
{
	if (ppInput) {
		// Lock the handle down
		reinterpret_cast<Handle_t *>(ppInput)->m_uFlags |= LOCKED;
		return reinterpret_cast<Handle_t *>(ppInput)->m_pData;
	}
	return NULL;
}

/*! ************************************

	\brief Clear the lock flag to a given handle

	\note This is a boolean flag, not reference counted
	
	\param ppInput Pointer to handle to unlock or \ref NULL
	\sa Burger::MemoryManagerHandle::Lock(void **ppInput)
	
***************************************/

void BURGER_API Burger::MemoryManagerHandle::Unlock(void **ppInput)
{
	if (ppInput) {
		// Clear the lock flag
		reinterpret_cast<Handle_t *>(ppInput)->m_uFlags &=(~LOCKED);
	}
}

/*! ************************************

	\brief Set a user supplied ID value for a handle
	
	\param ppInput Pointer to handle to set the ID
	\param uID Handle ID
	
***************************************/

void BURGER_API Burger::MemoryManagerHandle::SetID(void **ppInput,Word uID)
{
	if (ppInput) {
		// Clear the lock flag
		reinterpret_cast<Handle_t *>(ppInput)->m_uID = uID;
	}
}

/*! ************************************

	\brief Set the purge flag to a given handle
	\param ppInput Handle to allocated memory or \ref NULL
	\param uFlag \ref TRUE to enable purging, \ref FALSE to disable purging

***************************************/

void BURGER_API Burger::MemoryManagerHandle::SetPurgeFlag(void **ppInput,Word uFlag)
{
	if (ppInput) {
		Handle_t *pHandle = reinterpret_cast<Handle_t*>(ppInput);
		if (!(pHandle->m_uFlags & MALLOC)) {
			// Was it purgable?
			if (pHandle->m_pNextPurge) {
				// Unlink from the purge fifo
				pHandle->m_pPrevPurge->m_pNextPurge = pHandle->m_pNextPurge;
				pHandle->m_pNextPurge->m_pPrevPurge = pHandle->m_pPrevPurge;
			}
			// Now is it purgable?

			if (uFlag) {
				pHandle->m_pPrevPurge = &m_PurgeHandleFiFo;
				pHandle->m_pNextPurge = m_PurgeHandleFiFo.m_pNextPurge;
				m_PurgeHandleFiFo.m_pNextPurge->m_pPrevPurge = pHandle;
				m_PurgeHandleFiFo.m_pNextPurge = pHandle;
				return;
			}
			pHandle->m_pNextPurge = NULL;
			pHandle->m_pPrevPurge = NULL;
		}
	}
}

/*! ************************************

	\brief Get the current purge and lock flags of the handle
	\param ppInput Pointer to valid handle. \ref NULL is invalid
	\return All the flags from the memory handle. Mask with \ref LOCKED to
	check only for memory being locked
	
	\sa Burger::MemoryManagerHandle::SetLockedState(void **,Word)
	
***************************************/

Word BURGER_API Burger::MemoryManagerHandle::GetLockedState(void **ppInput)
{
	return reinterpret_cast<const Handle_t *>(ppInput)->m_uFlags;
}


/*! ************************************

	\brief Set the current purge and lock flags of the handle
	\param ppInput Handle to valid memory.
	\param uFlag \ref PURGABLE and \ref LOCKED are the only valid input flags
	\sa Burger::MemoryManagerHandle::GetLockedState(void **)
	
***************************************/

void BURGER_API Burger::MemoryManagerHandle::SetLockedState(void **ppInput,Word uFlag)
{
	uFlag &= (~(PURGABLE|LOCKED));
	Handle_t *pHandle = reinterpret_cast<Handle_t *>(ppInput);
	pHandle->m_uFlags = (pHandle->m_uFlags&(~(PURGABLE|LOCKED))) | uFlag;

	if (!(pHandle->m_uFlags & MALLOC)) {
		if (pHandle->m_pNextPurge) {	/* Was it purgable? */

		// Unlink from the purge fifo

			pHandle->m_pPrevPurge->m_pNextPurge = pHandle->m_pNextPurge;
			pHandle->m_pNextPurge->m_pPrevPurge = pHandle->m_pPrevPurge;
		}
		// Now is it purgable?

		if (uFlag&PURGABLE) {
			pHandle->m_pPrevPurge = &m_PurgeHandleFiFo;
			pHandle->m_pNextPurge = m_PurgeHandleFiFo.m_pNextPurge;
			m_PurgeHandleFiFo.m_pNextPurge->m_pPrevPurge = pHandle;
			m_PurgeHandleFiFo.m_pNextPurge = pHandle;
			return;
		}
		pHandle->m_pNextPurge = NULL;
		pHandle->m_pPrevPurge = NULL;
	}
}

/*! ************************************

	\brief Move a handle into the purged list 
	
	This routine will move a handle from the used list
	into the purged handle list. The handle is not discarded.
	This is the only way a handle can be placed into the purged list.

	It will call Burger::MemoryManagerHandle::ReleaseMemoryRange() to alert the free memory
	list that there is new free memory.
	
	\param ppInput Handle to allocated memory or \ref NULL to perform no action
	\sa Burger::MemoryManagerHandle::PurgeHandles(WordPtr)
	
***************************************/

void BURGER_API Burger::MemoryManagerHandle::Purge(void **ppInput)
{
	Handle_t *pHandle = reinterpret_cast<Handle_t *>(ppInput);
	if (pHandle &&		/* Valid pointer? */
		pHandle->m_pData &&
		!(pHandle->m_uFlags&MALLOC)) {		// Not purged?

		if (m_MemPurgeCallBack) {
			m_MemPurgeCallBack(m_pMemPurge,StagePurge);	/* I will purge now! */
		}

		pHandle->m_uFlags &= (~LOCKED);	/* Force unlocked */

		// Unlink from the purge list

		Handle_t *pPrev;			// Previous link
		Handle_t *pNext = pHandle->m_pNextPurge;	/* Forward link */
		if (pNext) {
			pPrev = pHandle->m_pPrevPurge;	/* Backward link */
			pNext->m_pPrevPurge = pPrev;	/* Unlink me from the list */
			pPrev->m_pNextPurge = pNext;
			pHandle->m_pNextPurge = NULL;
			pHandle->m_pPrevPurge = NULL;
		}

		// Unlink from the used list

		pNext = pHandle->m_pNextHandle;	// Forward link
		pPrev = pHandle->m_pPrevHandle;	// Backward link
		pNext->m_pPrevHandle = pPrev;	// Unlink me from the list
		pPrev->m_pNextHandle = pNext;

		// Move to the purged handle list
		// Don't harm the flags or the length!!

		ReleaseMemoryRange(pHandle->m_pData,pHandle->m_uLength,pPrev);	/* Release the memory */

		pPrev = m_PurgeHands.m_pNextHandle;	/* Get the first link */
		pHandle->m_pData = NULL;		/* Zap the pointer (Purge list) */
		pHandle->m_pPrevHandle = &m_PurgeHands;	/* I am the parent */
		pHandle->m_pNextHandle = pPrev;	/* Link it to the purge list */
		pPrev->m_pPrevHandle = pHandle;
		m_PurgeHands.m_pNextHandle = pHandle;	/* Make as the new head */
	}
}

/*! ************************************

	\brief Purges handles until the amount of memory requested is freed

	Purges all handles that are purgable and are
	greater or equal to the amount of memory
	It will call Purge(void **) to alert the free memory
	list that there is free memory.

	\param uSize The number of bytes to recover before aborting
	\return \ref TRUE if ANY memory was purged, \ref FALSE is there was no memory to recover
	\sa Burger::MemoryManagerHandle::Purge(void **)

***************************************/

Word BURGER_API Burger::MemoryManagerHandle::PurgeHandles(WordPtr uSize)
{
	Word uResult = FALSE;
	// Index to the purgable handle list
	Handle_t *pHandle = m_PurgeHandleFiFo.m_pPrevPurge;
	// No purgable memory?
	if (pHandle!=&m_PurgeHandleFiFo) {
		// Follow the handle list
		do {
			// Preload next link
			Handle_t *pNext = pHandle->m_pPrevPurge;
			// Round up
			WordPtr uTempLen = (pHandle->m_uLength+(ALIGNMENT-1)) & (~(ALIGNMENT-1));
			// Force a purge
			Purge(reinterpret_cast<void **>(pHandle));
			uResult = TRUE;
			if (uTempLen>=uSize) {
				break;
			}
			uSize-=uTempLen;			// Remove this...
			pHandle = pNext;			// Get the next link
		} while (pHandle!=&m_PurgeHandleFiFo);	// At the end?
	}
	return uResult;
}

/*! ************************************

	\brief Compact all of the movable blocks together

	Packs all memory together to reduce or eliminate fragementation
	This doesn't alter the handle list in any way but it can move
	memory around to get rid of empty holes in the memory map.
	
	\sa Burger::MemoryManagerHandle::PurgeHandles(WordPtr) or Burger::MemoryManagerHandle::Purge(void **)
	
***************************************/

void BURGER_API Burger::MemoryManagerHandle::CompactHandles(void)
{
	// Index to the active handle list
	Handle_t *pHandle = m_LowestUsedMemory.m_pNextHandle;
	// Failsafe
	if (pHandle!=&m_HighestUsedMemory) {	
		// Assume bogus
		Word bCalledCallBack = TRUE;		
		if (m_MemPurgeCallBack) {
			// Valid pointer?
			bCalledCallBack = FALSE;
		}
		// Skip all locked or fixed handles
		do {
			if (!(pHandle->m_uFlags & (LOCKED|FIXED))) {
				// Get the previous handle
				Handle_t *pPrev = pHandle->m_pPrevHandle;
				// Pad to long word
				WordPtr uSize = (pPrev->m_uLength+(ALIGNMENT-1))&(~(ALIGNMENT-1));
				Word8 *pStartMem = static_cast<Word8 *>(pPrev->m_pData) + uSize;
				// Any space here?
				uSize = static_cast<WordPtr>(static_cast<Word8 *>(pHandle->m_pData) - pStartMem);
				// If there is free space, then pack them
				if (uSize) {
					// Hadn't called it yet?
					if (!bCalledCallBack) {
						bCalledCallBack = TRUE;
						// Alert the app
						m_MemPurgeCallBack(m_pMemPurge,StageCompact);
					}
					// Save old address
					void *pTemp = pHandle->m_pData;
					// Set new address
					pHandle->m_pData = pStartMem;	
					// Release the memory
					ReleaseMemoryRange(pTemp,pHandle->m_uLength,pPrev);
					// Grab the memory again
					GrabMemoryRange(pStartMem,pHandle->m_uLength,pHandle,NULL);
					// Move the unpadded length
					MemoryMove(pStartMem,pTemp,pHandle->m_uLength);
				}
			}
			// Next handle in chain
			pHandle = pHandle->m_pNextHandle;
		} while (pHandle!=&m_HighestUsedMemory);
	}
}

/*! ************************************

	\brief Display all the memory
	
	\sa Burger::Debug::String(const char *)

***************************************/

void BURGER_API Burger::MemoryManagerHandle::DumpHandles(void)
{
	WordPtr uSize = GetTotalFreeMemory();
	Debug::String("Total free mem with purging ");
	Debug::String(uSize);
	Debug::String("\nUsed handle list\n");
	
	// Set the flag here, Debug::String COULD allocate memory on some
	// file systems for directory caches.
	// As a result, the recurse flag being set could case a handle that needs
	// to be tracked to be removed from the tracking list and therefore
	// flag an error that doesn't exist
	
	PrintHandles(&m_LowestUsedMemory,&m_LowestUsedMemory,TRUE);
	Debug::String("Purged handle list\n");
	PrintHandles(m_PurgeHands.m_pNextHandle,&m_PurgeHands,FALSE);
	Debug::String("Free memory list\n");
	PrintHandles(m_FreeMemoryChunks.m_pNextHandle,&m_FreeMemoryChunks,FALSE);
}


/*! ************************************

	\class Burger::MemoryManagerGlobalHandle
	\brief Global Handle Memory Manager helper class
	
	This class is a helper that attaches a \ref Burger::MemoryManagerHandle
	class to the global memory manager. When this instance shuts down,
	it will remove itself from the global memory manager.

	\sa Burger::GlobalMemoryManager and Burger::MemoryManagerHandle

***************************************/

/*! ************************************

	\brief Attaches a \ref Burger::MemoryManagerHandle class to the global memory manager.

	When this class is created, it will automatically attach itself
	to the global memory manager.

***************************************/

Burger::MemoryManagerGlobalHandle::MemoryManagerGlobalHandle(WordPtr uDefaultMemorySize,Word uDefaultHandleCount,WordPtr uMinReserveSize) :
	MemoryManagerHandle(uDefaultMemorySize,uDefaultHandleCount,uMinReserveSize)
{
	GlobalMemoryManager::Init(this);
}

/*! ************************************

	\brief Releases a Burger::MemoryManagerGlobalHandle class from the global memory manager.

	When this class is released, it will automatically remove itself
	to the global memory manager.

***************************************/

Burger::MemoryManagerGlobalHandle::~MemoryManagerGlobalHandle()
{
	GlobalMemoryManager::Shutdown();
}

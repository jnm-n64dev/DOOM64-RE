/* Z_zone.c */

#include "doomdef.h"

/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

It is of no value to free a cachable block, because it will get overwritten
automatically if needed

==============================================================================
*/

#define MINFRAGMENT 64

extern u32 NextFrameIdx;
extern u64 mem_heap[MEM_HEAP_SIZE / sizeof(u64)];

memzone_t *mainzone;

/*
========================
=
= Z_Init
=
========================
*/
void Z_Init(void)
{
	byte *mem;
	int size;

	mem = (byte *)(u32)(((u32)mem_heap + 15) & ~15);
	size = (u32)(mem_heap + (MEM_HEAP_SIZE / sizeof(u64))) - (u32)mem;

	mainzone = Z_InitZone(mem, size);
}

/*
========================
=
= Z_InitZone
=
========================
*/
memzone_t *Z_InitZone(byte *base, int size)
{
	memzone_t *zone;

	zone = (memzone_t *)base;

	zone->size = size;

	zone->rover = &zone->blocklist;
	zone->rover2 = &zone->blocklist;
	zone->rover3 = &zone->blocklist;

	zone->blocklist.size = size - (int)((byte *)&zone->blocklist - (byte *)zone);
	zone->blocklist.user = NULL;
	zone->blocklist.tag = 0;
	zone->blocklist.id = ZONEID;
	zone->blocklist.next = NULL;
	zone->blocklist.prev = NULL;

	return zone;
}

/*
========================
=
= Z_SetAllocBase
= Exclusive Doom64
=
========================
*/
void Z_SetAllocBase(memzone_t *mainzone)
{
	mainzone->rover2 = mainzone->rover;
}

/*
========================
=
= Z_Malloc2
=
= You can pass a NULL user if the tag is < PU_PURGELEVEL
========================
*/
void *Z_Malloc2(memzone_t *mainzone, int size, int tag, void *user)
{
	int extra;
	memblock_t *start, *rover, *newblock, *base;

#ifdef DEBUG_
	Z_CheckZone(mainzone);
#endif

	/* */
	/* scan through the block list looking for the first free block */
	/* of sufficient size, throwing out any purgable blocks along the way */
	/* */

	size += sizeof(memblock_t); /* account for size of block header */
	size = (size + 15) & ~15;	/* phrase align everything */

	start = base = mainzone->rover;

	while (base->user || base->size < size)
	{
		if (base->user)
		{
			rover = base;
		}
		else
		{
			rover = base->next;
		}
		if (!rover)
		{
			goto backtostart;
		}
		if (rover->user)
		{
			if (!(rover->tag & PU_PURGELEVEL))
			{
				if (!(rover->tag & PU_CACHE) || (u32)rover->lockframe >= (NextFrameIdx - 1))
				{
					/* hit an in use block, so move base past it */
					base = rover->next;
					if (!base)
					{
					backtostart:
						base = mainzone->rover2;
					}

					if (base == start) /* scaned all the way around the list */
					{
						Z_DumpHeap(mainzone);
						I_Error("Z_Malloc2: failed allocation on %i", size);
					}
					continue;
				}
			}

			/* free the rover block (adding the size to base) */
			Z_Free((byte *)rover + sizeof(memblock_t)); /* mark as free */
		}

		if (base != rover)
		{
			/* merge with base */
			base->size += rover->size;
			base->next = rover->next;
			if (rover->next)
			{
				rover->next->prev = base;
			}
			else
			{
				mainzone->rover3 = base;
			}
		}
	}

	/* */
	/* found a block big enough */
	/* */
	extra = base->size - size;
	if (extra > MINFRAGMENT)
	{
		/* there will be a free fragment after the allocated block */
		newblock = (memblock_t *)((byte *)base + size);
		newblock->prev = base;
		newblock->next = base->next;
		if (newblock->next)
		{
			newblock->next->prev = newblock;
		}
		else
		{
			mainzone->rover3 = newblock;
		}
		base->next = newblock;
		base->size = size;

		newblock->size = extra;
		newblock->user = NULL; /* free block */
		newblock->tag = 0;
		newblock->id = ZONEID;
	}

	if (user)
	{
		base->user = user; /* mark as an in use block */
		*(void **)user = (void *)((byte *)base + sizeof(memblock_t));
	}
	else
	{
		if (tag >= PU_PURGELEVEL)
		{
			I_Error("Z_Malloc: an owner is required for purgable blocks");
		}
		base->user = (void *)1; /* mark as in use, but unowned	 */
	}

	base->tag = tag;
	base->id = ZONEID;
	base->lockframe = NextFrameIdx;

	mainzone->rover = base->next; /* next allocation will start looking here */
	if (!mainzone->rover)
	{
		mainzone->rover3 = base;
		mainzone->rover = mainzone->rover2;
	}

#ifdef DEBUG_
	Z_CheckZone(mainzone);
#endif

	return (void *)((byte *)base + sizeof(memblock_t));
}

/*
========================
=
= Z_Alloc2
=
= You can pass a NULL user if the tag is < PU_PURGELEVEL
= Exclusive Psx Doom
========================
*/
void *Z_Alloc2(memzone_t *mainzone, int size, int tag, void *user) // 8002CBE0
{
	int extra;
	memblock_t *rover, *base, *block, *newblock;

#ifdef DEBUG_
	Z_CheckZone(mainzone);
#endif

	/* scan through the block list looking for the first free block */
	/* of sufficient size, throwing out any purgable blocks along the way */

	size += sizeof(memblock_t); /* account for size of block header */
	size = (size + 15) & ~15;	/* phrase align everything */

	base = mainzone->rover3;

	while (base->user || base->size < size)
	{
		if (base->user)
		{
			rover = base;
		}
		else
		{
			/* hit an in use block, so move base past it */
			rover = base->prev;
			if (!rover)
			{
				Z_DumpHeap(mainzone);
				I_Error("Z_Alloc: failed allocation on %i", size);
			}
		}

		if (rover->user)
		{
			if (!(rover->tag & PU_PURGELEVEL))
			{
				if (!(rover->tag & PU_CACHE) || (u32)rover->lockframe >= (NextFrameIdx - 1))
				{
					/* hit an in use block, so move base past it */
					base = rover->prev;
					if (!base)
					{
						Z_DumpHeap(mainzone);
						I_Error("Z_Alloc: failed allocation on %i", size);
					}
					continue;
				}
			}

			/* free the rover block (adding the size to base) */
			Z_Free((byte *)rover + sizeof(memblock_t)); /* mark as free */
		}

		if (base != rover)
		{
			/* merge with base */
			rover->size += base->size;
			rover->next = base->next;

			if (base->next)
			{
				base->next->prev = rover;
			}
			else
			{
				mainzone->rover3 = rover;
			}
			base = rover;
		}
	}

	/* found a block big enough */
	extra = (base->size - size);

	newblock = base;
	block = base;

	if (extra > MINFRAGMENT)
	{
		block = (memblock_t *)((byte *)base + extra);
		block->prev = newblock;
		block->next = (void *)newblock->next;

		if (newblock->next)
		{
			newblock->next->prev = block;
		}
		newblock->next = block;
		block->size = size;
		newblock->size = extra;
		newblock->user = 0;
		newblock->tag = 0;
		newblock->id = ZONEID;
	}

	if (block->next == 0)
	{
		mainzone->rover3 = block;
	}

	if (user)
	{
		block->user = user; /* mark as an in use block */
		*(void **)user = (void *)((byte *)block + sizeof(memblock_t));
	}
	else
	{
		if (tag >= PU_PURGELEVEL)
		{
			I_Error("Z_Alloc: an owner is required for purgable blocks");
		}
		block->user = (void *)1; /* mark as in use, but unowned	 */
	}

	block->id = ZONEID;
	block->tag = tag;
	block->lockframe = NextFrameIdx;

#ifdef DEBUG_
	Z_CheckZone(mainzone);
#endif

	return (void *)((byte *)block + sizeof(memblock_t));
}

/*
========================
=
= Z_Free2
=
========================
*/
void Z_Free2(memzone_t *mainzone, void *ptr)
{
	memblock_t *block;

	block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
	{
		I_Error("Z_Free: freed a pointer without ZONEID");
	}
	if (block->user > (void **)0x100) /* smaller values are not pointers */
	{
		*block->user = 0; /* clear the user's mark */
	}
	block->user = NULL; /* mark as free */
	block->tag = 0;
}

/*
========================
=
= Z_FreeTags
=
========================
*/
void Z_FreeTags(memzone_t *mainzone, int tag) // 8002CE8C
{
	memblock_t *block, *next;

	for (block = &mainzone->blocklist; block; block = next)
	{
		/* get link before freeing */
		next = block->next;

		/* free block */
		if (block->user)
		{
			if (block->tag & tag)
			{
				Z_Free((byte *)block + sizeof(memblock_t));
			}
		}
	}

	for (block = &mainzone->blocklist; block; block = next)
	{
		/* get link before freeing */
		next = block->next;

		if (!block->user && next && !next->user)
		{
			block->size += next->size;
			block->next = next->next;
			if (next->next)
			{
				next->next->prev = block;
			}
			next = block;
		}
	}

	mainzone->rover = &mainzone->blocklist;
	mainzone->rover2 = &mainzone->blocklist;
	mainzone->rover3 = &mainzone->blocklist;

	block = mainzone->blocklist.next;
	while (block)
	{
		mainzone->rover3 = block;
		block = block->next;
	}
}

/*
========================
=
= Z_Touch
= Exclusive Doom64
=
========================
*/
void Z_Touch(void *ptr)
{
	memblock_t *block;

	block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
	{
		I_Error("Z_Touch: touched a pointer without ZONEID");
	}
	block->lockframe = NextFrameIdx;
}

/*
========================
=
= Z_CheckZone
=
========================
*/
void Z_CheckZone(memzone_t *mainzone)
{
	memblock_t *checkblock;
	int size;

	for (checkblock = &mainzone->blocklist; checkblock; checkblock = checkblock->next)
	{
		if (checkblock->id != ZONEID)
		{
			I_Error("Z_CheckZone: block missing ZONEID");
		}
		if (!checkblock->next)
		{
			size = (byte *)checkblock + checkblock->size - (byte *)mainzone;
			if (size != mainzone->size)
			{
				I_Error("Z_CheckZone: zone size changed from %d to %d\n", mainzone->size, size);
			}
			break;
		}

		if ((byte *)checkblock + checkblock->size != (byte *)checkblock->next)
		{
			I_Error("Z_CheckZone: block size does not touch the next block\n");
		}
		if (checkblock->next->prev != checkblock)
		{
			I_Error("Z_CheckZone: next block doesn't have proper back link\n");
		}
	}

#ifdef DEBUG_
	Z_DumpHeap(mainzone);
#endif
}

/*
========================
=
= Z_ChangeTag
=
========================
*/
void Z_ChangeTag(void *ptr, int tag)
{
	memblock_t *block;

	block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
	{
		I_Error("Z_ChangeTag: freed a pointer without ZONEID");
	}
	if (tag >= PU_PURGELEVEL && (int)block->user < 0x100)
	{
		I_Error("Z_ChangeTag: an owner is required for purgable blocks");
	}
	block->tag = tag;
	block->lockframe = NextFrameIdx;
}

/*
========================
=
= Z_FreeMemory
=
========================
*/
int Z_FreeMemory(memzone_t *mainzone)
{
	memblock_t *block;
	int free;

	free = 0;
	for (block = &mainzone->blocklist; block; block = block->next)
	{
		if (!block->user)
		{
			free += block->size;
		}
	}

	return free;
}

/*
========================
=
= Z_DumpHeap
=
========================
*/
void Z_DumpHeap(memzone_t *mainzone)
{
#ifdef DEBUG_
	memblock_t *block;

	printf("zone size: %i  location: %p\n", mainzone->size, mainzone);

	for (block = &mainzone->blocklist; block; block = block->next)
	{
		printf("block:%p    size:%7i    user:%p    tag:%3i    frame:%i\n",
			   block, block->size, block->user, block->tag, block->lockframe);

		if (!block->next)
		{
			continue;
		}
		if ((byte *)block + block->size != (byte *)block->next)
		{
			printf("ERROR: block size does not touch the next block\n");
		}
		if (block->next->prev != block)
		{
			printf("ERROR: next block doesn't have proper back link\n");
		}
	}
#endif
}

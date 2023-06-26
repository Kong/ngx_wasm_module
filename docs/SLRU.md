# The SLRU eviction algorithm

WasmX introduces a new eviction policy for shm-based key-value stores: SLRU
(which you may read as "Slot-aware LRU" or "Slab-aware LRU").

## Motivation

The `shm_kv` shared-memory key-value store facility is provided by WasmX as a
way for Wasm instances running on different workers to share data. This is
exposed to Wasm code via the [Proxy-Wasm SDK] through functions such as
`proxy_get_shared_data` and `proxy_set_shared_data`.

Since such shared-memory stores are often used to implement caches, it is useful
to have an automatic eviction policy attached to them to transparently handle
the case when the memory zone is full. To that effect, the [OpenResty]
implementation of shared-memory stores implements an LRU (Least-Recently Used)
eviction policy by maintaining a queue. This queue tracks the recency of
accesses for each item, so that when more room is needed to store a new item,
one or more of the least-recently accessed items can be freed.

This approach works well when all items stored in the shared-memory zone are of
similar sizes because the underlying data structure used to manage the Nginx
memory zone is a "slab allocator". A slab allocator manages a number of memory
pages and divides them into "slabs", where each slab is further split into
entries of the same size. This reduces internal fragmentation since the number
of entries stored in a slab is predictable, without large variance of free space
as items get added and removed.

When a slab-based shared-memory zone is used to store items of different sizes
however, this causes slabs with different item sizes to be produced. The Nginx
allocator organizes these different kinds of slabs into "slots", each of them
corresponding to a binary order of magnitude (2^3 = 8, 2^3 = 16, and so on).
Removing items from a slab of a given slot size makes space for a new item of
that size, but does not help in making space for an item of a different size,
unless all items of that slab get removed and the whole slab gets redirected to
the correct slot for that item's size.

This means that, if you have items of different sizes in your LRU queue, you
might remove many items and still have no impact into making enough room for
your new item. The OpenResty LRU eviction logic gives up after 30 items are
removed.

## The SLRU algorithm

The solution for minimizing deallocations is to keep separate LRU queues for
items of the same binary order of magnitude (2^n) in size. This matches the
slot-picking strategy used by the Nginx slab allocator. An independent LRU queue
is used within each slab.

In practice, this means that when a key-value store contains multiple types of
items of various sizes, a full memory zone running an SLRU eviction cycle will
most likely deallocate the least-recently-used item of that same size magnitude.
In the best case scenario, freeing up a single entry from a full slab for that
size, rather than deallocating many small items until an entire slab of an
incompatible size is freed and reassigned to the necessary size.

When the LRU queue for the item's size is empty, the SLRU algorithm will attempt
the next larger-size queue in order to minimize the number of elements
deallocated. If still no large-enough slot is available, the queues for the
largest smaller sizes will be attempted in descending order.

### In more detail

It is easier to understand the mechanics of the eviction algorithm once we
consider those of the slab allocator itself. In pseudo-code, this is a slightly
simplified view of how the Nginx slab allocator works on its own:

```
Determine the item's slot based on its size.

IF Is there a slab allocated at that slot size?
AND Is there an empty entry in that slab?
THEN GOTO <Success>

IF Can the memory zone allocate a new slab?
THEN GOTO <Allocate>
ELSE Fail.

<Allocate>:
   Allocate the new slab and add it to the slot.
   GOTO <Success>

<Success>:
   Store the item in that slab entry.
   Done.
```

And this is how the SLRU algorithm works in conjunction with the Nginx allocator
logic. We replace the `Fail.` condition above with a new operation, `<SLRU
Evict>`:

```
Determine the item's slot based on its size.

IF Is there a slab allocated at that slot size?
AND Is there an empty entry in that slab?
THEN GOTO <Success>

IF Can the memory zone allocate a new slab?
THEN GOTO <Allocate>
ELSE GOTO <SLRU Evict>

<SLRU Evict>:
   IF Is there an entry in that slot's LRU queue?
   THEN Remove the least-recently used item from that slot's queue.
        This opens up an item entry.
        GOTO <Success>

   IF Are there any larger slots in use?
   THEN REPEAT
            Remove the least-recently used item from the
            smallest of the larger slot sizes in use.
        UNTIL an entire slab gets freed.
        GOTO <Allocate>

   IF Are there any smaller slots in use?
   THEN REPEAT
            Remove the least-recently used items from the
            largest of the smaller slot sizes in use.
        UNTIL an entire slab gets freed.
        GOTO <Allocate>
   ELSE Fail.

<Allocate>:
   Allocate the new slab and add it to the slot.
   GOTO <Success>

<Success>:
   Store the item in that slab entry.
   Push the item key in the slot's LRU queue.
   Done.
```

[Proxy-Wasm]: PROXY_WASM.md
[OpenResty]: https://openresty.org/en/

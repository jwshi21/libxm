/*
 * Copyright (c) 2014-2017 Ilya Kaliman
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef WITH_MPI
#include <mpi.h>
#endif

#ifdef HAVE_BITSTRING_H
#include <bitstring.h>
#else
#include "compat/bitstring.h"
#endif

#ifdef HAVE_TREE_H
#include <sys/tree.h>
#else
#include "compat/tree.h"
#endif

#include "alloc.h"
#include "util.h"

#define XM_PAGE_SIZE (512ULL * 1024)
#define XM_GROW_SIZE (256ULL * 1024 * 1024 * 1024)

struct block {
	uintptr_t               data_ptr;
	size_t                  size_bytes;
	RB_ENTRY(block)         entry;
};

RB_HEAD(tree, block);

struct xm_allocator {
	int                     fd;
	int                     mpirank;
	char                   *path;
	size_t                  file_bytes;
	bitstr_t               *pages;
#ifdef _OPENMP
	omp_lock_t		mutex;
#endif
	struct tree             blocks;
};

static int
tree_cmp(const struct block *a, const struct block *b)
{
	return (a->data_ptr == b->data_ptr ? 0 :
	    a->data_ptr < b->data_ptr ? -1 : 1);
}

#ifndef __unused
#if defined(__GNUC__)
#define __unused __attribute__((__unused__))
#else
#define __unused
#endif
#endif /* __unused */
RB_GENERATE_STATIC(tree, block, entry, tree_cmp)

static struct block *
find_block(struct tree *tree, uintptr_t data_ptr)
{
	struct block key, *block;

	key.data_ptr = data_ptr;
	block = RB_FIND(tree, tree, &key);

	return (block);
}

static int
extend_file(xm_allocator_t *allocator)
{
	size_t oldsize, newsize;

	oldsize = bitstr_size(allocator->file_bytes / XM_PAGE_SIZE);
	allocator->file_bytes = allocator->file_bytes > XM_GROW_SIZE ?
	    allocator->file_bytes + XM_GROW_SIZE :
	    allocator->file_bytes * 2;
	newsize = bitstr_size(allocator->file_bytes / XM_PAGE_SIZE);

	if (ftruncate(allocator->fd, (off_t)allocator->file_bytes)) {
		perror("ftruncate");
		return (1);
	}
	if ((allocator->pages = realloc(allocator->pages,
	    newsize * sizeof(bitstr_t))) == NULL) {
		perror("realloc");
		return (1);
	}
	memset(allocator->pages + oldsize, 0,
	    (newsize - oldsize) * sizeof(bitstr_t));
	return (0);
}

static uintptr_t
find_pages(xm_allocator_t *allocator, size_t n_pages)
{
	int i, n_free, n_total, offset, start;

	assert(n_pages > 0);

	n_total = (int)(allocator->file_bytes / XM_PAGE_SIZE);
	bit_ffc(allocator->pages, n_total, &start);
	if (start == -1)
		return (XM_NULL_PTR);
	for (i = start, n_free = 0; i < n_total; i++) {
		if (bit_test(allocator->pages, i))
			n_free = 0;
		else
			n_free++;
		if (n_free == (int)n_pages) {
			offset = i + 1 - n_free;
			bit_nset(allocator->pages, offset, i);
			return ((uintptr_t)offset * XM_PAGE_SIZE);
		}
	}
	return (XM_NULL_PTR);
}

static uintptr_t
allocate_pages(xm_allocator_t *allocator, size_t size_bytes)
{
	size_t n_pages;
	uintptr_t ptr;

	if (size_bytes == 0)
		return (XM_NULL_PTR);

	n_pages = (size_bytes + XM_PAGE_SIZE - 1) / XM_PAGE_SIZE;

	while ((ptr = find_pages(allocator, n_pages)) == XM_NULL_PTR)
		if (extend_file(allocator))
			return (XM_NULL_PTR);
	return (ptr);
}

xm_allocator_t *
xm_allocator_create(const char *path)
{
	xm_allocator_t *allocator;

	if ((allocator = calloc(1, sizeof(*allocator))) == NULL) {
		perror("malloc");
		return (NULL);
	}
#ifdef WITH_MPI
	MPI_Comm_rank(MPI_COMM_WORLD, &allocator->mpirank);
#endif
	if (path) {
		allocator->file_bytes = XM_PAGE_SIZE;
		allocator->pages = bit_alloc(1);

		if (allocator->mpirank == 0) {
			if ((allocator->fd = open(path, O_CREAT|O_RDWR,
			    S_IRUSR|S_IWUSR)) == -1) {
				perror("open");
				free(allocator);
				return (NULL);
			}
			if (ftruncate(allocator->fd,
			    (off_t)allocator->file_bytes)) {
				perror("ftruncate");
				if (close(allocator->fd))
					perror("close");
				free(allocator);
				return (NULL);
			}
#ifdef WITH_MPI
			MPI_Barrier(MPI_COMM_WORLD);
#endif
		} else {
#ifdef WITH_MPI
			MPI_Barrier(MPI_COMM_WORLD);
#endif
			if ((allocator->fd = open(path, O_RDWR)) == -1) {
				perror("open");
				free(allocator);
				return (NULL);
			}
		}
		if ((allocator->path = strdup(path)) == NULL) {
			perror("malloc");
			if (close(allocator->fd))
				perror("close");
			free(allocator);
			return (NULL);
		}
	}
#ifdef _OPENMP
	omp_init_lock(&allocator->mutex);
#endif
	RB_INIT(&allocator->blocks);
	return (allocator);
}

const char *
xm_allocator_get_path(xm_allocator_t *allocator)
{
	return (allocator->path);
}

uintptr_t
xm_allocator_allocate(xm_allocator_t *allocator, size_t size_bytes)
{
	struct block *block;
	void *data;
	uintptr_t data_ptr = XM_NULL_PTR;

	if (allocator->mpirank != 0) {
#ifdef WITH_MPI
		MPI_Bcast(&data_ptr, 1, MPI_UNSIGNED_LONG_LONG, 0,
		    MPI_COMM_WORLD);
#endif
		return (data_ptr);
	}
	if ((block = calloc(1, sizeof(*block))) == NULL) {
		perror("malloc");
		return (XM_NULL_PTR);
	}
#ifdef _OPENMP
	omp_set_lock(&allocator->mutex);
#endif
	if (allocator->path) {
		if ((data_ptr = allocate_pages(allocator,
		    size_bytes)) == XM_NULL_PTR)
			goto fail;
		block->data_ptr = data_ptr;
	} else {
		if ((data = malloc(size_bytes)) == NULL) {
			perror("malloc");
			goto fail;
		}
		block->data_ptr = (uintptr_t)data;
	}

	block->size_bytes = size_bytes;
	RB_INSERT(tree, &allocator->blocks, block);
#ifdef _OPENMP
	omp_unset_lock(&allocator->mutex);
#endif
#ifdef WITH_MPI
	MPI_Bcast(&data_ptr, 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
#endif
	return (block->data_ptr);
fail:
#ifdef _OPENMP
	omp_unset_lock(&allocator->mutex);
#endif
	free(block);
	return (XM_NULL_PTR);
}

void
xm_allocator_read(xm_allocator_t *allocator, uintptr_t data_ptr,
    void *mem, size_t size_bytes)
{
	ssize_t read_bytes;
	off_t offset;

	assert(data_ptr != XM_NULL_PTR);

	if (allocator->path == NULL) {
		memcpy(mem, (const void *)data_ptr, size_bytes);
		return;
	}

	offset = (off_t)data_ptr;
	read_bytes = pread(allocator->fd, mem, size_bytes, offset);
	if (read_bytes != (ssize_t)size_bytes)
		fatal("pread");
}

void
xm_allocator_write(xm_allocator_t *allocator, uintptr_t data_ptr,
    const void *mem, size_t size_bytes)
{
	ssize_t write_bytes;
	off_t offset;

	assert(data_ptr != XM_NULL_PTR);

	if (allocator->path == NULL) {
		memcpy((void *)data_ptr, mem, size_bytes);
		return;
	}

	offset = (off_t)data_ptr;
	write_bytes = pwrite(allocator->fd, mem, size_bytes, offset);
	if (write_bytes != (ssize_t)size_bytes)
		fatal("pwrite");
}

void
xm_allocator_deallocate(xm_allocator_t *allocator, uintptr_t data_ptr)
{
	struct block *block;

	if (allocator->mpirank != 0)
		return;
	if (data_ptr == XM_NULL_PTR)
		return;
#ifdef _OPENMP
	omp_set_lock(&allocator->mutex);
#endif
	block = find_block(&allocator->blocks, data_ptr);
	assert(block);

	RB_REMOVE(tree, &allocator->blocks, block);

	if (allocator->path) {
		int start, count;
		assert(data_ptr % XM_PAGE_SIZE == 0);
		start = (int)(data_ptr / XM_PAGE_SIZE);
		count = (int)((block->size_bytes - 1) / XM_PAGE_SIZE);
		bit_nclear(allocator->pages, start, start + count);
	} else {
		free((void *)data_ptr);
	}
	free(block);
#ifdef _OPENMP
	omp_unset_lock(&allocator->mutex);
#endif
}

void
xm_allocator_destroy(xm_allocator_t *allocator)
{
	struct block *block, *next;

	if (allocator == NULL)
		return;
	if (allocator->mpirank == 0) {
		RB_FOREACH_SAFE(block, tree, &allocator->blocks, next) {
			xm_allocator_deallocate(allocator, block->data_ptr);
		}
		if (allocator->path) {
			if (close(allocator->fd))
				perror("close");
			if (unlink(allocator->path))
				perror("unlink");
		}
	}
#ifdef _OPENMP
	omp_destroy_lock(&allocator->mutex);
#endif
	free(allocator->path);
	free(allocator->pages);
	free(allocator);
}

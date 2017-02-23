/*
 * Copyright (c) 2014-2016 Ilya Kaliman <ilya.kaliman@gmail.com>
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

#ifndef XM_AUXIL_H
#define XM_AUXIL_H

#include "xm.h"

#define XM_INIT_NONE          0  /* Do not initialize memory. */
#define XM_INIT_ZERO          1  /* Initialize with zeros. */
#define XM_INIT_RAND          2  /* Initialize with random data. */

#define XM_RESULT_SUCCESS     0  /* No error. */
#define XM_RESULT_NO_MEMORY   1  /* Cannot allocate memory. */

int xm_aux_init(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_oo(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_ov(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_vv(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_vvx(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_oooo(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_ooov(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_oovv(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_ovov(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_ovvv(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_vvvv(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_ooovvv(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_13(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_13c(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_14(struct xm_tensor *, struct xm_allocator *, size_t, int);
int xm_aux_init_14b(struct xm_tensor *, struct xm_allocator *, size_t, int);
xm_scalar_t xm_random_scalar(void);

#endif /* XM_AUXIL_H */
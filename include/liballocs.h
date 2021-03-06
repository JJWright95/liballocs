#ifndef LIBALLOCS_H_
#define LIBALLOCS_H_

#ifndef _GNU_SOURCE
#warning "compilation unit is not _GNU_SOURCE; some features liballocs requires may not be available"
#endif

#ifdef __cplusplus
extern "C" {
typedef bool _Bool;
#define INLINE inline
#else
#define INLINE inline __attribute__((gnu_inline))
#endif

#include <sys/types.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <link.h>
#include "heap_index.h"
#include "pageindex.h"

extern void warnx(const char *fmt, ...); // avoid repeating proto
#ifndef NDEBUG
#include <assert.h>
#endif

#include "uniqtype.h"

#define ALLOC_IS_DYNAMICALLY_SIZED(all, as) \
	((all) != (as))

/* ** begin added for inline get_alloc_info */
#ifdef USE_REAL_LIBUNWIND
#include <libunwind.h>
#else
#include "fake-libunwind.h"
#endif

#include "allocsmt.h"

extern unsigned long __liballocs_aborted_stack;
extern unsigned long __liballocs_aborted_static;
extern unsigned long __liballocs_aborted_unknown_storage;
extern unsigned long __liballocs_hit_heap_case;
extern unsigned long __liballocs_hit_stack_case;
extern unsigned long __liballocs_hit_static_case;
extern unsigned long __liballocs_aborted_unindexed_heap;
extern unsigned long __liballocs_aborted_unrecognised_allocsite;

/* This API is a mess because there are three different classes of client. 
 * 
 * - extenders (libcrunch)
 * - direct clients (programs linking -lallocs and using our API) 
 * - weak clients (programs that can use liballocs, but run okay without)
 * 
 * The first two are the ones who'll instantiate our inlines and hence
 * generate references to our stuff. Weak clients will just (perhaps)
 * embed our CIL inlines. So it's only stuff in the liballocs_cil_inlines.h 
 * header file that they depend on. We deliberately keep this small, and
 * ideally it will run even without the noop library (i.e. never branch
 * out of line), but the linker currently won't generate the right code
 * without the noop library being present.
 */

// stuff for use by extenders only -- direct/weak clients shouldn't use this
struct addrlist;
int __liballocs_addrlist_contains(struct addrlist *l, void *addr);
void __liballocs_addrlist_add(struct addrlist *l, void *addr);
extern struct addrlist __liballocs_unrecognised_heap_alloc_sites;

Dl_info dladdr_with_cache(const void *addr);

extern void *__liballocs_main_bp; // beginning of main's stack frame
char *get_exe_fullname(void) __attribute__((visibility("hidden")));
char *get_exe_basename(void) __attribute__((visibility("hidden")));

extern inline struct uniqtype *allocsite_to_uniqtype(const void *allocsite) __attribute__((gnu_inline,always_inline));
extern inline struct uniqtype * __attribute__((gnu_inline)) allocsite_to_uniqtype(const void *allocsite)
{
	if (!allocsite) return NULL;
	assert(__liballocs_allocsmt != NULL);
	struct allocsite_entry **bucketpos = ALLOCSMT_FUN(ADDR, allocsite);
	struct allocsite_entry *bucket = *bucketpos;
	for (struct allocsite_entry *p = bucket; p; p = (struct allocsite_entry *) p->next)
	{
		if (p->allocsite == allocsite)
		{
			return p->uniqtype;
		}
	}
	return NULL;
}

extern inline _Bool 
__attribute__((always_inline,gnu_inline))
__liballocs_first_subobject_spanning(
	signed *p_target_offset_within_uniqtype,
	struct uniqtype **p_cur_obj_uniqtype,
	struct uniqtype **p_cur_containing_uniqtype,
	struct uniqtype_rel_info **p_cur_contained_pos) __attribute__((always_inline,gnu_inline));
/* ** end added for inline get_alloc_info */

extern int __liballocs_debug_level;
extern _Bool __liballocs_is_initialized __attribute__((weak));

int __liballocs_global_init(void) __attribute__((weak));
// declare as const void *-returning, to simplify trumptr
const void *__liballocs_typestr_to_uniqtype(const char *typestr) __attribute__((weak));
void *__liballocs_my_typeobj(void) __attribute__((weak));

/* Uniqtypes for signed_char and unsigned_char and so on.
 * 
 * These are part of the API, BUT we don't want them to appear in the preload .so 
 * because then they can't be uniqued w.r.t. the executable.
 * So they go in a nasty .a, and the -lallocs .so is a linker script.
 * That way, an executable linking -lallocs will get them, together with
 * a reference to the liballocs .so. The .so does not define them.
 */

extern struct uniqtype __uniqtype__void/* __attribute__((weak))*/;
extern struct uniqtype __uniqtype__int/* __attribute__((weak))*/;
extern struct uniqtype __uniqtype__unsigned_int/* __attribute__((weak))*/;
extern struct uniqtype __uniqtype__signed_char/* __attribute__((weak))*/;
extern struct uniqtype __uniqtype__unsigned_char/* __attribute__((weak))*/;
extern struct uniqtype __uniqtype____FUN_FROM___FUN_TO_uint$64 /* __attribute__((weak))*/;
// #pragma 
#define __liballocs_uniqtype_of_typeless_functions __uniqtype____FUN_FROM___FUN_TO_uint$64
extern struct uniqtype __uniqtype__long_int;
extern struct uniqtype __uniqtype__unsigned_long_int;
extern struct uniqtype __uniqtype__short_int;
extern struct uniqtype __uniqtype__short_unsigned_int;
extern struct uniqtype __uniqtype____PTR_void;
extern struct uniqtype __uniqtype____PTR_signed_char;
extern struct uniqtype __uniqtype__float;
extern struct uniqtype __uniqtype__double;

#ifdef __GNUC__ /* HACK. FIXME: why do we need this? maybe libfootprints uses them? */
extern struct uniqtype __uniqtype__float$32;
extern struct uniqtype __uniqtype__float$64;
extern struct uniqtype __uniqtype__int$16;
extern struct uniqtype __uniqtype__int$32;
extern struct uniqtype __uniqtype__int$64;
extern struct uniqtype __uniqtype__uint$16;
extern struct uniqtype __uniqtype__uint$32;
extern struct uniqtype __uniqtype__uint$64;
extern struct uniqtype __uniqtype__signed_char$8;
extern struct uniqtype __uniqtype__unsigned_char$8;
extern struct uniqtype __uniqtype____PTR_int$32;
extern struct uniqtype __uniqtype____PTR_int$64;
extern struct uniqtype __uniqtype____PTR_uint$32;
extern struct uniqtype __uniqtype____PTR_uint$64;
extern struct uniqtype __uniqtype____PTR_signed_char$8;
#endif

struct liballocs_err;
typedef struct liballocs_err *liballocs_err_t;

extern struct liballocs_err __liballocs_err_stack_walk_step_failure;
extern struct liballocs_err __liballocs_err_stack_walk_reached_higher_frame;
extern struct liballocs_err __liballocs_err_stack_walk_reached_top_of_stack;
extern struct liballocs_err __liballocs_err_unknown_stack_walk_problem;
extern struct liballocs_err __liballocs_err_unindexed_heap_object;
extern struct liballocs_err __liballocs_err_unrecognised_alloc_site;
extern struct liballocs_err __liballocs_err_unrecognised_static_object;
extern struct liballocs_err __liballocs_err_object_of_unknown_storage;

const char *__liballocs_errstring(struct liballocs_err *err);
liballocs_err_t extract_and_output_alloc_site_and_type(
    struct insert *p_ins,
    struct uniqtype **out_type,
    void **out_site
) __attribute__((visibility("hidden")));


/* We define a dladdr that caches stuff. */
Dl_info dladdr_with_cache(const void *addr);

/* Iterate over all uniqtypes in a given shared object. */
int __liballocs_iterate_types(void *typelib_handle, 
		int (*cb)(struct uniqtype *t, void *arg), void *arg);
/* Our main API: query allocation information for a pointer */
inline struct liballocs_err *__liballocs_get_alloc_info(const void *obj, 
	struct allocator **out_allocator, const void **out_alloc_start,
	unsigned long *out_alloc_size_bytes,
	struct uniqtype **out_alloc_uniqtype, const void **out_alloc_site);
extern INLINE _Bool __liballocs_find_matching_subobject(signed target_offset_within_uniqtype,
	struct uniqtype *cur_obj_uniqtype, struct uniqtype *test_uniqtype, 
	struct uniqtype **last_attempted_uniqtype, signed *last_uniqtype_offset,
		signed *p_cumulative_offset_searched,
		struct uniqtype **p_cur_containing_uniqtype,
		struct uniqtype_rel_info **p_cur_contained_pos) __attribute__((hot));
/* Some inlines follow at the bottom. */

/* Public API for l0index / mappings was here. FIXME: why was it public? Presumably
 * for libcrunch's consumption, i.e. clients that "extend". But libcrunch
 * already includes liballocs_private.h, so there's no need for this to be here. 
 * I've moved it to liballocs_private.h. */

#include "allocmeta.h"

/* our own private assert */
extern inline void
__attribute__((always_inline,gnu_inline))
__liballocs_private_assert (_Bool cond, const char *reason, 
	const char *f, unsigned l, const char *fn)
{
#ifndef NDEBUG
	if (!cond) __assert_fail(reason, f, l, fn);
#endif
}

extern inline void 
__attribute__((always_inline,gnu_inline))
__liballocs_ensure_init(void)
{
	//__liballocs_private_assert(__liballocs_check_init() == 0, "liballocs init", 
	//	__FILE__, __LINE__, __func__);
	if (__builtin_expect(! & __liballocs_is_initialized, 0))
	{
		/* This means that we're not linked with libcrunch. 
		 * There's nothing we can do! */
		__liballocs_private_assert(0, "liballocs presence", 
			__FILE__, __LINE__, __func__);
	}
	if (__builtin_expect(!__liballocs_is_initialized, 0))
	{
		/* This means we haven't initialized.
		 * Try that now (it won't try more than once). */
		int ret = __liballocs_global_init();
		__liballocs_private_assert(ret == 0, "liballocs init", 
			__FILE__, __LINE__, __func__);
	}
}

/* Here "walk" is primarily walking "down". We do a little walking along,
 * in the case of unions. We do both iteratively. */
extern inline int 
__liballocs_walk_subobjects_spanning(
	const signed target_offset_within_u,
	struct uniqtype *u, 
	int (*cb)(struct uniqtype *spans, signed span_start_offset, unsigned depth,
		struct uniqtype *containing, struct uniqtype_rel_info *contained_pos, 
		signed containing_span_start_offset, void *arg),
	void *arg
	)
__attribute__((always_inline,gnu_inline));

inline int 
__liballocs_walk_subobjects_spanning_rec(
	signed accum_offset, unsigned accum_depth,
	const signed target_offset_within_u,
	struct uniqtype *u, 
	int (*cb)(struct uniqtype *spans, signed span_start_offset, unsigned depth,
		struct uniqtype *containing, struct uniqtype_rel_info *contained_pos, 
		signed containing_span_start_offset, void *arg),
	void *arg
	);

extern inline int 
__attribute__((always_inline,gnu_inline))
__liballocs_walk_subobjects_spanning(
	const signed target_offset_within_u,
	struct uniqtype *u, 
	int (*cb)(struct uniqtype *spans, signed span_start_offset, unsigned depth,
		struct uniqtype *containing, struct uniqtype_rel_info *contained_pos, 
		signed containing_span_start_offset, void *arg),
	void *arg
	)
{
	return __liballocs_walk_subobjects_spanning_rec(
		/* accum_offset */ 0, /* accum_depth */ 0, 
		target_offset_within_u, u, cb, arg);
}

inline int 
__liballocs_walk_subobjects_spanning_rec(
	signed accum_offset, unsigned accum_depth,
	const signed target_offset_within_u,
	struct uniqtype *u, 
	int (*cb)(struct uniqtype *spans, signed span_start_offset, unsigned depth, 
		struct uniqtype *containing, struct uniqtype_rel_info *contained_pos, 
		signed containing_span_start_offset, void *arg),
	void *arg
	)
{
	signed ret = 0;
	/* Calculate the offset to descend to, if any. This is different for 
	 * structs versus arrays. */
	if (UNIQTYPE_IS_ARRAY_TYPE(u))
	{
		struct uniqtype_rel_info *element_contained_in_u = &u->related[0];
		struct uniqtype *element_uniqtype = element_contained_in_u->un.t.ptr;
		signed div = target_offset_within_u / element_uniqtype->pos_maxoff;
		signed mod = target_offset_within_u % element_uniqtype->pos_maxoff;
		if (element_uniqtype->pos_maxoff != 0 && UNIQTYPE_ARRAY_LENGTH(u) > div)
		{
			signed skip_sz = div * element_uniqtype->pos_maxoff;
			__liballocs_private_assert(target_offset_within_u < u->pos_maxoff,
				"offset not within subobject", __FILE__, __LINE__, __func__);
			int ret = cb(element_uniqtype, accum_offset + skip_sz, accum_depth + 1u, 
				u, element_contained_in_u, accum_offset, arg);
			if (ret) return ret;
			// tail-recurse
			else return __liballocs_walk_subobjects_spanning_rec(
				accum_offset + (div * element_uniqtype->pos_maxoff), accum_depth + 1u,
				mod, element_uniqtype, cb, arg);
		} 
		else // element's pos_maxoff == 0 || num_contained <= target_offset / element's pos_maxoff 
		{}
	}
	else // struct/union case
	{
		signed num_contained = UNIQTYPE_COMPOSITE_MEMBER_COUNT(u);
		int lower_ind = 0;
		int upper_ind = num_contained;
		while (lower_ind + 1 < upper_ind) // difference of >= 2
		{
			/* Bisect the interval */
			int bisect_ind = (upper_ind + lower_ind) / 2;
			__liballocs_private_assert(bisect_ind > lower_ind, "progress", __FILE__, __LINE__, __func__);
			if (u->related[bisect_ind].un.memb.off > target_offset_within_u)
			{
				/* Our solution lies in the lower half of the interval */
				upper_ind = bisect_ind;
			} else lower_ind = bisect_ind;
		}

		if (lower_ind + 1 == upper_ind)
		{
			/* We found one offset; we may still have overshot, in the case of a 
			 * stack frame where offset zero might not be used. */
			if (u->related[lower_ind].un.memb.off > target_offset_within_u)
			{
				return 0; // FIXME: cb not called; what should our return value be then?
			}

			/* ... but we might not have found the *lowest* index, in the 
			 * case of a union. Scan backwards so that we have the lowest. 
			 * FIXME: need to account for the element size? Or here are we
			 * ignoring padding anyway? */
			signed offset = u->related[lower_ind].un.memb.off;
			for (;
					lower_ind > 0 && u->related[lower_ind-1].un.memb.off == offset;
					--lower_ind);
			// now we have the lowest lower_ind
			// scan forwards!
			for (int i_ind = lower_ind; i_ind < UNIQTYPE_COMPOSITE_MEMBER_COUNT(u)
					&& u->related[i_ind].un.memb.off == offset;
					++i_ind)
			{
				if (target_offset_within_u < u->pos_maxoff)
				{
					int ret = cb(u->related[i_ind].un.memb.ptr, accum_offset + offset,
							accum_depth + 1u, u, &u->related[i_ind], accum_offset, arg);
					if (ret) return ret;
					else
					{
						ret = __liballocs_walk_subobjects_spanning_rec(
							accum_offset + offset, accum_depth + 1u,
							target_offset_within_u - offset, u->related[i_ind].un.memb.ptr, cb, arg);
						if (ret) return ret;
					}
				}
			}
		}
		else /* lower_ind >= upper_ind */
		{
			// this should mean num_contained == 0
			__liballocs_private_assert(num_contained == 0,
				"no contained objects", __FILE__, __LINE__, __func__);
		}
	}
	
	return ret;
}

extern inline _Bool 
__liballocs_first_subobject_spanning(
	signed *p_target_offset_within_uniqtype,
	struct uniqtype **p_cur_obj_uniqtype,
	struct uniqtype **p_cur_containing_uniqtype,
	struct uniqtype_rel_info **p_cur_contained_pos) __attribute__((always_inline,gnu_inline));

extern inline _Bool 
__attribute__((always_inline,gnu_inline))
__liballocs_first_subobject_spanning(
	signed *p_target_offset_within_uniqtype,
	struct uniqtype **p_cur_obj_uniqtype,
	struct uniqtype **p_cur_containing_uniqtype,
	struct uniqtype_rel_info **p_cur_contained_pos)
{
	struct uniqtype *cur_obj_uniqtype = *p_cur_obj_uniqtype;
	signed target_offset_within_uniqtype = *p_target_offset_within_uniqtype;
	/* Calculate the offset to descend to, if any. This is different for 
	 * structs versus arrays. */
	if (UNIQTYPE_IS_ARRAY_TYPE(cur_obj_uniqtype))
	{
		signed num_contained = UNIQTYPE_ARRAY_LENGTH(cur_obj_uniqtype);
		struct uniqtype *element_uniqtype = UNIQTYPE_ARRAY_ELEMENT_TYPE(cur_obj_uniqtype);
		if (element_uniqtype->pos_maxoff != 0 && 
				num_contained > target_offset_within_uniqtype / element_uniqtype->pos_maxoff)
		{
			*p_cur_containing_uniqtype = cur_obj_uniqtype;
			*p_cur_contained_pos = &cur_obj_uniqtype->related[0];
			*p_cur_obj_uniqtype = element_uniqtype;
			*p_target_offset_within_uniqtype = target_offset_within_uniqtype % element_uniqtype->pos_maxoff;
			return 1;
		} else return 0;
	}
	else // struct/union case
	{
		signed num_contained = UNIQTYPE_COMPOSITE_MEMBER_COUNT(cur_obj_uniqtype);

		int lower_ind = 0;
		int upper_ind = num_contained;
		while (lower_ind + 1 < upper_ind) // difference of >= 2
		{
			/* Bisect the interval */
			int bisect_ind = (upper_ind + lower_ind) / 2;
			__liballocs_private_assert(bisect_ind > lower_ind, "bisection progress", 
				__FILE__, __LINE__, __func__);
			if (cur_obj_uniqtype->related[bisect_ind].un.memb.off > target_offset_within_uniqtype)
			{
				/* Our solution lies in the lower half of the interval */
				upper_ind = bisect_ind;
			} else lower_ind = bisect_ind;
		}

		if (lower_ind + 1 == upper_ind)
		{
			/* We found one offset; we may still have overshot, in the case of a 
			 * stack frame where offset zero might not be used. */
			if (cur_obj_uniqtype->related[lower_ind].un.memb.off > target_offset_within_uniqtype)
			{
				return 0;
			}
			/* We found one offset */
			__liballocs_private_assert(cur_obj_uniqtype->related[lower_ind].un.memb.off <= target_offset_within_uniqtype,
				"offset underapproximates", __FILE__, __LINE__, __func__);

			/* ... but we might not have found the *lowest* index, in the 
			 * case of a union. Scan backwards so that we have the lowest. 
			 * FIXME: need to account for the element size? Or here are we
			 * ignoring padding anyway? */
			while (lower_ind > 0 
				&& cur_obj_uniqtype->related[lower_ind-1].un.memb.off
					 == cur_obj_uniqtype->related[lower_ind].un.memb.off)
			{
				--lower_ind;
			}
			*p_cur_contained_pos = &cur_obj_uniqtype->related[lower_ind];
			*p_cur_containing_uniqtype = cur_obj_uniqtype;
			*p_cur_obj_uniqtype
			 = cur_obj_uniqtype->related[lower_ind].un.memb.ptr;
			/* p_cur_obj_uniqtype now points to the subobject's uniqtype. 
			 * We still have to adjust the offset. */
			*p_target_offset_within_uniqtype
			 = target_offset_within_uniqtype - cur_obj_uniqtype->related[lower_ind].un.memb.off;

			return 1;
		}
		else /* lower_ind >= upper_ind */
		{
			// this should mean num_contained == 0
			__liballocs_private_assert(num_contained == 0,
				"no contained objects", __FILE__, __LINE__, __func__);
			return 0;
		}
	}
}

#ifndef __cplusplus
extern 
#endif
inline
_Bool 
__liballocs_find_matching_subobject(signed target_offset_within_uniqtype,
	struct uniqtype *cur_obj_uniqtype, struct uniqtype *test_uniqtype, 
	struct uniqtype **last_attempted_uniqtype, signed *last_uniqtype_offset,
		signed *p_cumulative_offset_searched,
		struct uniqtype **p_cur_containing_uniqtype,
		struct uniqtype_rel_info **p_cur_contained_pos)
#ifndef __cplusplus
__attribute__((gnu_inline))
#endif
;

#ifndef __cplusplus
extern 
__attribute__((gnu_inline))
#endif
inline
_Bool 
__liballocs_find_matching_subobject(signed target_offset_within_uniqtype,
	struct uniqtype *cur_obj_uniqtype, struct uniqtype *test_uniqtype, 
	struct uniqtype **last_attempted_uniqtype, signed *last_uniqtype_offset,
		signed *p_cumulative_offset_searched,
		struct uniqtype **p_cur_containing_uniqtype,
		struct uniqtype_rel_info **p_cur_contained_pos)
{
	if (target_offset_within_uniqtype == 0 
		&& (!test_uniqtype || cur_obj_uniqtype == test_uniqtype))
	{
		if (p_cur_containing_uniqtype) *p_cur_containing_uniqtype = NULL;
		if (p_cur_contained_pos) *p_cur_contained_pos = NULL;
		return 1;
	}
	else
	{
		/* We might have *multiple* subobjects spanning the offset. 
		 * Test all of them. */
		struct uniqtype *containing_uniqtype = NULL;
		struct uniqtype_rel_info *contained_pos = NULL;
		
		signed sub_target_offset = target_offset_within_uniqtype;
		struct uniqtype *contained_uniqtype = cur_obj_uniqtype;
		
		_Bool success = __liballocs_first_subobject_spanning(
			&sub_target_offset, &contained_uniqtype,
			&containing_uniqtype, &contained_pos);
		// now we have a *new* sub_target_offset and contained_uniqtype
		
		if (!success) return 0;
		
		if (p_cumulative_offset_searched) *p_cumulative_offset_searched += contained_pos->un.memb.off;
		
		if (last_attempted_uniqtype) *last_attempted_uniqtype = contained_uniqtype;
		if (last_uniqtype_offset) *last_uniqtype_offset = sub_target_offset;
		do {
			assert(containing_uniqtype == cur_obj_uniqtype);
			_Bool recursive_test = __liballocs_find_matching_subobject(
					sub_target_offset,
					contained_uniqtype, test_uniqtype, 
					last_attempted_uniqtype, last_uniqtype_offset, p_cumulative_offset_searched,
					p_cur_containing_uniqtype,
					p_cur_contained_pos);
			if (__builtin_expect(recursive_test, 1))
			{
				if (p_cur_containing_uniqtype) *p_cur_containing_uniqtype = containing_uniqtype;
				if (p_cur_contained_pos) *p_cur_contained_pos = contained_pos;

				return 1;
			}
			// else look for a later contained subobject at the same offset
			signed subobj_ind = contained_pos - &containing_uniqtype->related[0];
			assert(subobj_ind >= 0);
			assert(subobj_ind == 0 || subobj_ind < UNIQTYPE_COMPOSITE_MEMBER_COUNT(containing_uniqtype));
			if (__builtin_expect(
					UNIQTYPE_COMPOSITE_MEMBER_COUNT(containing_uniqtype) <= subobj_ind + 1
					|| containing_uniqtype->related[subobj_ind + 1].un.memb.off != 
						containing_uniqtype->related[subobj_ind].un.memb.off,
				1))
			{
				// no more subobjects at the same offset, so fail
				return 0;
			} 
			else
			{
				contained_pos = &containing_uniqtype->related[subobj_ind + 1];
				contained_uniqtype = contained_pos->un.memb.ptr;
			}
		} while (1);
		
		assert(0);
	}
}

/* HACK HACK HACKETY HACK: we want our fast-path functions to be inlined.
 * However, there's a linking problem: we reference pageindex which is a protected
 * symbol. From an executable that is a client of liballocs, under the small code
 * model, the linker won't let us reference this symbol (copy-reloc'ing a protected
 * symbol is a recipe for trouble, which ld.bfd sensibly does not allow). So we
 * don't inline this function from such clients. We use a hacky method for identifying
 * them: non-PIC code. This BREAKS small-model PIE executables -- use -mcmodel=large
 * if you want a PIE executable that is a client of liballocs. FIXME: I'm not actually
 * sure that sensible things happen under the large code model -- more experimentation
 * required.
 */
#if defined(__PIC__) || defined(__code_model_large__)
inline 
struct liballocs_err *
__liballocs_get_alloc_info
	(const void *obj, 
	struct allocator **out_allocator,
	const void **out_alloc_start,
	unsigned long *out_alloc_size_bytes, 
	struct uniqtype **out_alloc_uniqtype, 
	const void **out_alloc_site)
{
	struct liballocs_err *err = 0;

	struct big_allocation *containing_bigalloc;
	struct big_allocation *maybe_the_allocation;
	struct allocator *a = __liballocs_leaf_allocator_for(obj, &containing_bigalloc, &maybe_the_allocation);
	if (__builtin_expect(!a, 0))
	{
		_Bool fixed = __liballocs_notify_unindexed_address(obj);
		if (fixed)
		{
			a = __liballocs_leaf_allocator_for(obj, &containing_bigalloc, &maybe_the_allocation);
			if (!a) abort();
		}
		else
		{
			__liballocs_report_wild_address(obj);
			++__liballocs_aborted_unknown_storage;
			return &__liballocs_err_object_of_unknown_storage;
		}
	}
	if (out_allocator) *out_allocator = a;
	return a->get_info((void*) obj, maybe_the_allocation, out_alloc_uniqtype, (void**) out_alloc_start,
			out_alloc_size_bytes, out_alloc_site);
}
#else
struct liballocs_err *
__liballocs_get_alloc_info
	(const void *obj, 
	struct allocator **out_allocator,
	const void **out_alloc_start,
	unsigned long *out_alloc_size_bytes, 
	struct uniqtype **out_alloc_uniqtype, 
	const void **out_alloc_site);
#endif

/* We define a more friendly API for simple queries.
 * NOTE that we don't make these functions inline. They are still fast, internally,
 * because they make an inlined call to __liballocs_get_alloc_info.
 * BUT we don't want to make them inline themselves, because this complicates linking
 * to liballocs quite a bit. Specifically, if we inline them into callers, then 
 * callers need to link against lots of internals of liballocs which would otherwise
 * have hidden visibility. We would have to add mocked-up versions of all this stuff
 * to the noop library if we wanted this to work. Recall also that linking -lallocs does
 * *not* work! You really need to preload liballocs for it to work. */

struct uniqtype * 
__liballocs_get_alloc_type(void *obj);

struct uniqtype * 
__liballocs_get_outermost_type(void *obj);

struct uniqtype * 
__liballocs_get_type_inside(void *obj, struct uniqtype *t);

struct uniqtype * 
__liballocs_get_innermost_type(void *obj);

struct insert *__liballocs_get_insert(const void *mem); // HACK: please remove (see libcrunch)

inline 
const char **__liballocs_uniqtype_subobject_names(struct uniqtype *t)
{
	/* HACK: this all needs to go away, once we overhaul uniqtype's layout. */
	Dl_info i = dladdr_with_cache(t);
	if (i.dli_sname)
	{
		char *names_name = alloca(strlen(i.dli_sname) + sizeof "_subobj_names" + 1); /* HACK: necessary? */
		strncpy(names_name, i.dli_sname, strlen(i.dli_sname));
		strcat(names_name, "_subobj_names");
		void *handle = dlopen(i.dli_fname, RTLD_NOW | RTLD_NOLOAD);
		if (handle)
		{
			const char **names_name_array = (const char**) dlsym(handle, names_name);
			dlclose(handle);
			return names_name_array;
		}
	}
	return NULL;
}

// struct uniqtype * 
// get_outermost_type(void *obj, struct uniqtype *bound)
// {
// 	
// }
// 
void *
__liballocs_get_alloc_site(void *obj);
unsigned long
__liballocs_get_alloc_size(void *obj);

extern inline void *(__attribute__((gnu_inline,always_inline)) __liballocs_get_sp)(void);
extern inline void *(__attribute__((gnu_inline,always_inline)) __liballocs_get_sp)(void)
{
	unw_word_t sp;
#ifdef __i386__
	__asm__ ("movl %%esp, %0\n" :"=r"(sp));
#elif defined(__x86_64__)
	__asm__("movq %%rsp, %0\n" : "=r"(sp));
#else
#error "Unsupported architecture."
#endif
	return (void*) sp;
}

static inline int __liballocs_walk_stack(int (*cb)(void *, void *, void *, void *), void *arg)
{
	liballocs_err_t err;
	unw_cursor_t cursor, saved_cursor;
	unw_word_t higherframe_sp = 0, sp, higherframe_bp = 0, bp = 0, ip = 0, higherframe_ip = 0;
	int unw_ret;
	int ret = 0;
	
	/* Get an initial snapshot. */
	unw_context_t unw_context;
	unw_ret = unw_getcontext(&unw_context);
	unw_init_local(&cursor, &unw_context);

	unw_ret = unw_get_reg(&cursor, UNW_REG_SP, &higherframe_sp);
#ifndef NDEBUG
	assert(__liballocs_get_sp() == higherframe_sp);
#endif
	unw_ret = unw_get_reg(&cursor, UNW_REG_IP, &higherframe_ip);

	_Bool at_or_above_main = 0;
	do
	{
		// callee_ip = ip;
		// prev_saved_cursor is the cursor into the callee's frame 
		// prev_saved_cursor = saved_cursor; // FIXME: will be garbage if callee_ip == 0
		saved_cursor = cursor; // saved_cursor is the *current* frame's cursor

		/* First get the ip, sp and symname of the current stack frame. */
		unw_ret = unw_get_reg(&cursor, UNW_REG_IP, &ip); assert(unw_ret == 0);
		unw_ret = unw_get_reg(&cursor, UNW_REG_SP, &sp); assert(unw_ret == 0);
		// try to get the bp, but no problem if we don't
		unw_ret = unw_get_reg(&cursor, UNW_TDEP_BP, &bp); 
		_Bool got_higherframe_bp = 0;
		
		ret = cb((void*) ip, (void*) sp, (void*) bp, arg);
		if (ret) return ret;
		
		int step_ret = unw_step(&cursor);
		if (step_ret > 0)
		{
			unw_ret = unw_get_reg(&cursor, UNW_REG_SP, &higherframe_sp); assert(unw_ret == 0);
			unw_ret = unw_get_reg(&cursor, UNW_REG_IP, &higherframe_ip); assert(unw_ret == 0);
			// try to get the bp, but no problem if we don't
			unw_ret = unw_get_reg(&cursor, UNW_TDEP_BP, &higherframe_bp); 
			got_higherframe_bp = (unw_ret == 0) && higherframe_bp != 0;
		}
		else if (step_ret == 0)
		{
#define BEGINNING_OF_STACK ((uintptr_t) MAXIMUM_USER_ADDRESS)
			higherframe_sp = BEGINNING_OF_STACK;
			higherframe_bp = BEGINNING_OF_STACK;
			got_higherframe_bp = 1;
			higherframe_ip = 0x0;
		}
		else // step failure
		{
			// err = &__liballocs_err_stack_walk_step_failure;
			ret = -1;
			break;
		}
	} while (higherframe_sp != BEGINNING_OF_STACK);
#undef BEGINNING_OF_STACK
	
	return ret;
}
#ifdef __cplusplus
} // end extern "C"
#endif

#endif

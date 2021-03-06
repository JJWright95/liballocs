#ifndef LIBALLOCS_MAPS_H_
#define LIBALLOCS_MAPS_H_

#include <string.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#include <sys/user.h>
#else
#include <asm-generic/fcntl.h>
#endif

/* Don't include stdio -- trap-syscalls won't like it, for example. */
int sscanf(const char *str, const char *format, ...);
int open(const char *pathname, int flags, ...);

/* Rethinking this "maps" concept in the name of portability (to FreeBSD), we have
 * 
 * a "line" that is really a "raw entry" and read via sysctl() or read();
 * a "proc entry" which is our abstraction of a memory mapping.
 * 
 * Then we have some functions:
 * get_a_line really reads a single raw entry into the user's buffer;
 * process_one_maps_entry decodes a raw entry and calls the cb on the decoded entry;
 * for_each_maps_entry is a loop that interleaves get_a_line with process_one;
 *
 * In trap-syscalls we avoid race conditions by doing it differently: rather
 * than use for_each_maps_entry, we snapshot all the raw entries and then
 * call process_one on each.
 * 
 */

static inline intptr_t get_maps_handle(void)
{
#ifdef __FreeBSD__
	int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid() };
	size_t len;
	len = 0;
	int error = sysctl(name, sizeof name / sizeof name[0], NULL, &len, NULL, 0);
	if (error) return (intptr_t) NULL;
	/* Massive HACK: allow for 33% growth in the memory mapping count. libprocstat does this
	 * in FreeBSD, so it "must be okay". */
	size_t fudged_len = len * 4 / 3;
	char *buf = malloc(sizeof (off_t) + fudged_len);
	if (buf)
	{
		error = sysctl(name, sizeof name / sizeof name[0], buf + sizeof (off_t), &fudged_len, NULL, 0);
		if (error) 
		{
			free(buf);
			return (intptr_t) NULL;
		}
	}
	return buf;
	#if 0
		char *pos = buf + sizeof (off_t);
		size_t minimum_packed_struct_size = offsetof(struct kinfo_vmentry, kve_path);
		char **start_positions = malloc(sizeof (char*) * fudged_len / minimum_packed_struct_size);
		if (start_positions)
		{
			size_t *struct_sizes = malloc(sizeof (size_t) * fudged_len / minimum_packed_struct_size);
			if (struct_sizes)
			{
				while (pos < buf + sizeof (off_t) + fudged_len)
				{
					struct kinfo_vmentry *kv = (struct kinfo_vmentry *) pos;
					if (kv->kve_structsize == 0) break;
					pos += kv->kve_structsize;
					start_positions[cnt] = pos;
					struct_sizes[cnt] = kv->kve_structsize;
					cnt++;
				}
				/* We need to give the caller
				 * a single buffer that they can easily iterate through
				 * and then free in one go. 
				 * So we reallocate the buffer to the actual size required, 
				 * then work backwards to copy the packed structs onto
				 * the old storage. By the end we will be overwriting the 
				 * packed records. */
				buf = realloc(buf, cnt * sizeof (struct kinfo_vmentry));
				if (buf)
				{
					for (int i = cnt - 1; i >= 0; --i)
					{
						memcpy(((struct kinfo_vmentry *) buf) + i, start_positions[i], struct_sizes[i]);
					} 
					
				}
				
				free(struct_sizes);
			}
		
		
			free(start_positions);
		}
		


	kiv = calloc(cnt, sizeof(*kiv));
	if (kiv == NULL) {
		free(buf);
		return (NULL);
	}
	bp = buf;
	eb = buf + len;
	kp = kiv;
	/* Pass 2: unpack */
	while (bp < eb) {
		kv = (struct kinfo_vmentry *)(uintptr_t)bp;
		if (kv->kve_structsize == 0)
			break;
		/* Copy/expand into pre-zeroed buffer */
		memcpy(kp, kv, kv->kve_structsize);
		/* Advance to next packed record */
		bp += kv->kve_structsize;
		/* Set field size to fixed length, advance */
		kp->kve_structsize = sizeof(*kp);
		kp++;
	}
	free(buf);

	}
	
	#endif
#else
	return (intptr_t) open("/proc/self/maps", O_RDONLY);
#endif
}

static inline void free_maps_handle(intptr_t handle)
{
#ifdef __FreeBSD__
	free((void*) handle);
#else
	close(handle);
#endif
}

static inline ssize_t get_a_line(char *buf, size_t size, intptr_t handle)
{
#ifdef __FreeBSD__
	/* "Getting a line" just means reading one raw record into a buffer. */
	char *handle_buf_start =  (char*) handle + sizeof (off_t);
	char *handle_pos = handle_buf_start + *(off_t *)handle_buf_start;
	size_t sz = ((struct kinfo_vmentry *)(char*) handle)->kve_structsize;
	ssize_t actual_size_to_copy = (sz < size) ? sz : size;
	*(off_t *)handle_buf_start += actual_size_to_copy;
	memcpy(buf, (char*) handle_pos, actual_size_to_copy);
	return actual_size_to_copy;
#else
	if (size == 0) return -1; // now size is at least 1
	int fd = (int) handle;
	
	// read some stuff, at most `size - 1' bytes (we're going to add a null), into the buffer
	ssize_t bytes_read = read(fd, buf, size - 1);
	
	// if we got EOF and read zero bytes, return -1
	if (bytes_read == 0) return -1;
	
	// did we get enough that we have a whole line?
	char *found = memchr(buf, '\n', bytes_read);
	// if so, rewind the file to just after the newline
	if (found)
	{
		size_t end_of_newline_displacement = (found - buf) + 1;
		(void) lseek(fd, 
				end_of_newline_displacement - bytes_read /* i.e. negative if we read more */,
				SEEK_CUR);
		buf[end_of_newline_displacement] = '\0';
		return end_of_newline_displacement;
	}
	else
	{
		/* We didn't read enough. But that should only be because of EOF of error.
		 * So just return whatever we got. */
		buf[bytes_read] = '\0';
		return bytes_read;
	}
#endif
}
struct proc_entry
{
	unsigned long first, second;
	char r, w, x, p;
	unsigned offset;
	unsigned devmaj, devmin;
	unsigned inode;
	char rest[4096];
};
typedef int maps_cb_t(struct proc_entry *ent, char *linebuf, void *arg);

static inline int process_one_maps_entry(char *linebuf, struct proc_entry *entry_buf,
		maps_cb_t *cb, void *arg)
{
#ifdef __FreeBSD__
	struct kinfo_vmentry *kve = (struct kinfo_vmentry *) linebuf;
	/* Populate the entry buf with data from the kinfo_vmentry. */
	*entry_buf = (struct proc_entry) {
		.first = kve->kve_start,
		.second = kve->kve_end,
		.r = kve->kve_protection & KVME_PROT_READ ? 'r' : '-',
		.w = kve->kve_protection & KVME_PROT_WRITE ? 'w' : '-',
		.x = kve->kve_protection & KVME_PROT_EXEC ? 'x' : '-',
		.p = 'p' /* FIXME */,
		.offset = kve->kve_offset,
		.devmaj = 0 /* FIXME */,
		.devmin = 0 /* FIXME */,
		.inode = kve->kve_vn_fileid,
		.rest = kve->kve_path
	};
	
#else
	#define NUM_FIELDS 11
	entry_buf->rest[0] = '\0';
	int fields_read = sscanf(linebuf, 
		"%lx-%lx %c%c%c%c %8x %2x:%2x %d %4095[\x01-\x09\x0b-\xff]\n",
		&entry_buf->first, &entry_buf->second, &entry_buf->r, &entry_buf->w, &entry_buf->x, 
		&entry_buf->p, &entry_buf->offset, &entry_buf->devmaj, &entry_buf->devmin, 
		&entry_buf->inode, entry_buf->rest);

	assert(fields_read >= (NUM_FIELDS-1)); // we might not get a "rest"
	#undef NUM_FIELDS
#endif
	int ret = cb(entry_buf, linebuf, arg);
	if (ret) return ret;
	else return 0;
}

static inline int for_each_maps_entry(int fd, char *linebuf, size_t bufsz, struct proc_entry *entry_buf, 
		maps_cb_t *cb, void *arg)
{
	while (get_a_line(linebuf, bufsz, fd) != -1)
	{
		int ret = process_one_maps_entry(linebuf, entry_buf, cb, arg);
		if (ret) return ret;
	}
	return 0;
	
	/* Here's how we open-coded it in trap-syscalls's saw_mapping(), 
	 * before we started using selected C library calls in there.
void saw_mapping(const char *line_begin_pos, const char *line_end_pos)
{
	const char *line_pos = line_begin_pos;
	unsigned long begin_addr = read_hex_num(&line_pos, line_end_pos);
	++line_pos;
	unsigned long end_addr = read_hex_num(&line_pos, line_end_pos);
	++line_pos;
	char r = *line_pos++;
	char w = *line_pos++;
	char x = *line_pos++;
	char p __attribute__((unused)) = *line_pos++;
	const char *newline = strchr(line_pos, '\n');
	if (!newline) newline = line_pos;
	const char *slash = strchr(line_pos, '/');
	const char *filename_src = NULL;
	size_t filename_sz;
	if (slash && slash < newline && *(slash - 1) == ' ')
	{
		filename_src = slash;
		const char *filename_end = strchr(filename_src, '\n');
		if (!filename_end)
		{
			// hmm -- slash but no newline
			filename_end = line_end_pos;
		}
		filename_sz = filename_end - filename_src;
	}
	else
	{
		// no filename present
		filename_sz = 0;
		filename_src = line_pos;
	}
	
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
	char filename[PATH_MAX + 1];
	size_t copy_sz = (filename_sz < PATH_MAX) ? filename_sz : PATH_MAX;
	strncpy(filename, filename_src, copy_sz);
	filename[copy_sz] = '\0';
	 
	 */
}

#endif

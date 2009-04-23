#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <linux/fs.h>
typedef uint64_t __u64;
typedef uint32_t __u32;
#include "fiemap.h"
#include <errno.h>

#define FS_IOC_FIEMAP			_IOWR('f', 11, struct fiemap)

static int use_fibmap = 1;

uint64_t firstblock(const char *fname)
{
	int ret, fd;
	struct fiemap *fiemap;
	struct fiemap_extent *extent;
	unsigned long block = 0;
	char buf[sizeof(struct fiemap) + sizeof(struct fiemap_extent)];

	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		printf("Open failed: %s: %s\n", fname, strerror(errno)); 
		return 0;
	}

	if (use_fibmap) {
		ret = ioctl(fd, FIBMAP, &block);
		if (ret == 0) {
			close(fd);
			return block;
		}
		printf("FIBMAP failed: %s: %s\n", fname, strerror(errno)); 
		use_fibmap = 0;
	}

	fiemap = (struct fiemap *)buf;

	fiemap->fm_start = 0;
	fiemap->fm_length = 4096;
	fiemap->fm_flags = 0;
	fiemap->fm_extent_count = 1;

	ret = ioctl(fd, FS_IOC_FIEMAP, fiemap);
	if (ret == -1) {
		if (errno == EBADR) {
			printf("Kernel does not the flags: 0x%x\n",
				fiemap->fm_flags);
			close(fd);
			return -1;
		}
		printf("fiemap error %d: \"%s\"\n", errno,
			strerror(errno));
		close(fd);
		return 0;
	}


	close(fd);

	if (fiemap->fm_extent_count == 0 || fiemap->fm_mapped_extents == 0) {
		printf("extent_count == 0: %s\n", fname);
		return 0;
	}
	extent = &fiemap->fm_extents[0];
	return extent->fe_physical >> 4;
}

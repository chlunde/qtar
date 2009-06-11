#define _XOPEN_SOURCE 600

#include <vector>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <linux/types.h>
//#include <linux/dirent.h>
#include <linux/unistd.h>
#include <errno.h>

#include "linux_getdents.h"

#include <sys/syscall.h>   /* For SYS_xxx definitions */

static inline int linux_getdents(unsigned int fd, void *dirp, unsigned int count)
{
	return syscall(SYS_getdents, fd, dirp, count);
}

int
dir_read(std::string& path, std::vector<struct us_dirent>& dest)
{
	int fd = open(path.c_str(), O_DIRECTORY | O_RDONLY);
	int written;
	int offset = 0;

#define buf_size 4096
	char buf[buf_size];

	if (fd < 0) {
		err(EXIT_FAILURE, "open %s", path.c_str());
	}

	do {
		written = linux_getdents(fd, buf, buf_size);
		if (written < 0) {
			err(EXIT_FAILURE, "getdents %s", path.c_str());
		}

		offset = 0;

		while (offset < written) {
			struct linux_dirent *s = (struct linux_dirent*)(buf + offset);
			struct us_dirent d;
			char *type = buf + offset + s->d_reclen - 1;

			d.d_ino = s->d_ino;
			d.d_name = std::string(s->d_name);
			d.d_type = *type;
			d.parent = path;

			dest.push_back(d);
#if 0
			std::push_heap(dest.begin(), dest.end());
#endif

			offset += s->d_reclen;
		}
	} while (written > 0);

	close(fd);

	return 0;
}

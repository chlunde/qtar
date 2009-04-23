#ifndef __MY_LINUX_GETDENTS_H_
#define __MY_LINUX_GETDENTS_H_

#include <dirent.h>

#include <vector>
#include <string>

#include <err.h>

struct us_dirent {
	unsigned long d_ino;
	std::string d_name;
	std::string parent;
	char d_type;

	us_dirent() : d_ino(0), d_type(0), st_initialized(false) {}

	bool operator<(const struct us_dirent &other) const {
		return this->d_ino < other.d_ino;
	}

	bool operator>(const struct us_dirent &other) const {
		return this->d_ino > other.d_ino;
	}

	bool is_file() { return this->d_type == DT_REG; }
	bool is_dir() { return this->d_type == DT_DIR; }
	bool is_link() { return this->d_type == DT_LNK; }
	bool is_unknown() { return this->d_type == 0; }

	void set_is_file() { this->d_type = DT_REG; }
	void set_is_dir() { this->d_type = DT_DIR; }
	void set_is_link() { this->d_type = DT_LNK; }
	void set_is_unknown() { this->d_type = 0; }

	struct stat& stat() {
		if (! st_initialized) {
			std::string name = fq();
			st_initialized = true;
			if (lstat(name.c_str(), &st) < 0)
				err(EXIT_FAILURE, "stat %s failed", name.c_str());
			if (d_ino == 0)
				d_ino = st.st_ino;
		}

		return st;
	}

	std::string fq() {
		/*
		if (!parent.empty() && parent[parent.length() - 1] == '/')
			return parent + d_name;
		*/

		return parent + "/" + d_name;
	}

private:
	struct stat st;
	bool st_initialized;
};

struct linux_dirent {
	unsigned long  d_ino;     /* Inode number */
	unsigned long  d_off;     /* Offset to next dirent */
	unsigned short d_reclen;  /* Length of this dirent */
	char           d_name []; /* Filename (null-terminated) */
	/* length is actually (d_reclen - 2 - offsetof(struct linux_dirent, d_name) */
	/* char        pad;          Zero padding byte */
	/* char        d_type;       File type (only since Linux 2.6.4; offset is (d_reclen - 1)) */

	/*
	bool operator<(const struct linux_dirent *other) {
		return this->d_ino < other->d_ino;
	}

	*/
};

int
dir_read(std::string& parent, std::vector<struct us_dirent> &dest);

#endif

#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <deque>
#include <map>
#include <set>

#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

#include "linux_getdents.h"
#include "firstblock.h"

static bool use_getdents_type = true;

template<class T>
struct Queue {
	std::deque<T> queue;

	Queue() {
	}

	void push(T item) {
		queue.push_back(item);
	}

	T pop() {
		T ret = queue.front();
		queue.pop_front();
		return ret;
	}

	bool empty() {
		return queue.empty();
	}
};

template<class T>
struct CScan {
	std::set<T> elevator;
	uint64_t last_position;

	CScan() : last_position(0) {
	}

	void push(T item) {
		elevator.insert(item);
	}

	T pop() {
		typename std::set<T>::iterator it;
		T bound;
		bound.d_ino = last_position;

		it = elevator.lower_bound(bound);
		if (it == elevator.end()) {
			if (last_position == 0)
				throw new std::exception();//"pop on empty queue");
			last_position = 0;
			return pop();
		}

		T ret = *it;
		elevator.erase(it);
		last_position = ret.d_ino;
		return ret;
	}

	bool empty() {
		return elevator.empty();
	}
};

template<template <typename> class QueueType>
class Traverser {
	struct archive *a;
	std::string root;

public:
	Traverser(struct archive *a) : a(a) {}

	void walk(std::string& r) {
		root = r;

		struct us_dirent d;
		d.d_ino = -1;
		d.set_is_dir();
		d.parent = root;
		d.d_name = ".";
		d.stat();
		dirheap.push(d);

		while (!dirheap.empty()) {
			d = dirheap.pop();
			heapwalk(d);
		}

		while (!fileheap.empty()) {
			d = fileheap.pop();
			addfile(d);
		}
	}

private:
	//std::vector<struct us_dirent> queue;
	//TwoWayHeapElevator<struct us_dirent> heap;
	//
	QueueType<struct us_dirent> dirheap;
	QueueType<struct us_dirent> fileheap;

	std::map<uid_t, std::string> uidcache;
	std::map<gid_t, std::string> gidcache;

	std::string lookup_uname(uid_t uid) {
		std::map<uid_t, std::string>::iterator it = uidcache.find(uid);
		if (it == uidcache.end()) {
		       struct passwd *pwent = getpwuid(uid);
		       std::string uname;
		       if (pwent)
			       uname = pwent->pw_name;

		       uidcache.insert(std::pair<uid_t, std::string>(uid, uname));

		       return uname;
		}

		return (*it).second;
	}

	std::string lookup_gname(gid_t gid) {
		std::map<gid_t, std::string>::iterator it = gidcache.find(gid);
		if (it == gidcache.end()) {
		       struct group *grent = getgrgid(gid);
		       std::string gname;
		       if (grent)
			       gname = grent->gr_name;

		       gidcache.insert(std::pair<gid_t, std::string>(gid, gname));

		       return gname;
		}

		return (*it).second;
	}

	//QueueType heap;
	//
	void addpath(struct us_dirent& file) {
		std::string fq = file.fq();
		std::string rel = fq.substr(root.length() + 1);
		std::string uname, gname;
		const struct stat& st = file.stat();
		uname = lookup_uname(st.st_uid);
		gname = lookup_gname(st.st_gid);
#if 0
		std::cout << "addPATH " << fq << "\t" << file.d_ino << "\t" << file.is_file() << std::endl;
#endif
		struct archive_entry *entry = archive_entry_new();

		archive_entry_copy_stat(entry, &file.stat());
		if (uname.size())
			archive_entry_set_uname(entry, uname.c_str());
		if (gname.size())
			archive_entry_set_gname(entry, gname.c_str());
		archive_entry_set_pathname(entry, rel.c_str());
		int r = archive_write_header(a, entry);
		if (r != ARCHIVE_OK)
			errx(EXIT_FAILURE, "archive_write_header failed: %s", archive_error_string(a));
		archive_entry_free(entry);
	}

	void queuefile(struct us_dirent& file) {
		file.d_ino = firstblock(file.fq().c_str());

		fileheap.push(file);
	}

	void addfile(struct us_dirent& file) {
		std::string fq = file.fq();
		char buff[1024 * 128];
		std::string rel = fq.substr(root.length() + 1);
		std::string uname, gname;
		const struct stat& st = file.stat();
		uname = lookup_uname(st.st_uid);
		gname = lookup_gname(st.st_gid);
#if 0
		std::cout << "addfile " << fq << "\t" << file.d_ino << "\t" << file.is_file() << "\t" << uname << "\t" << gname << "\t" << rel << std::endl;
#endif

		struct archive_entry *entry = archive_entry_new();

		archive_entry_copy_stat(entry, &file.stat());
		if (uname.size())
			archive_entry_set_uname(entry, uname.c_str());
		if (gname.size())
			archive_entry_set_gname(entry, gname.c_str());
		archive_entry_set_pathname(entry, rel.c_str());
		int r = archive_write_header(a, entry);
		if (r != ARCHIVE_OK)
			errx(EXIT_FAILURE, "archive_write_header failed: %s", archive_error_string(a));

		//std::cout << file.d_name << std::endl;
		int fd = open(fq.c_str(), O_RDONLY);
		if (fd < 0)
			err(EXIT_FAILURE, "open(%s) failed", fq.c_str());

		ssize_t len = read(fd, buff, sizeof(buff));
		ssize_t written = 0;
		while (len > 0) {
			int bytes_written = archive_write_data(a, buff, len);
			written += bytes_written;
			//std::cout << "write " << len << " - " << bytes_written << std::endl;
			if (written < st.st_size)
				len = read(fd, buff, sizeof(buff));
			else
				break;
		}
		close(fd);
		archive_entry_free(entry);
	}

        void heapwalk(struct us_dirent& dirent) {
//		std::cout << "heapwalk " << dirent.d_ino << "\t" << dirent.d_name << std::endl;

                if (dirent.is_file() || dirent.is_link()) {
                        queuefile(dirent);
			return;
		}

		std::string path;
                if (dirent.d_name != ".") {
                        path = dirent.parent + "/" + dirent.d_name;
                } else  {
                        path = dirent.parent;
                }

#if 0
		static long long a;
		std::cout << (long long)(dirent.d_ino - a) << std::endl;
		a = dirent.d_ino;
#endif
		if (dirent.is_unknown()) {
			struct stat& buf = dirent.stat();

			if (S_ISDIR(buf.st_mode)) {
				dirent.set_is_dir();
			} else {
				dirent.set_is_file();
				queuefile(dirent);
				return;
			}
		}

		std::vector<struct us_dirent> l;
		if (dir_read(path, l) < 0) {
			//dirent.set_is_file();
			//addfile(dirent);
			std::cerr << "Error!" << std::endl;
			return;
		}

		sort(l.begin(), l.end());

		addpath(dirent);

		dirent.set_is_dir();
                //queue.push_back(dirent);

		for (std::vector<struct us_dirent>::iterator it = l.begin(); it != l.end(); ++it) {
			if (it->d_name == "." || it->d_name == "..")
				continue;

			if (!use_getdents_type)
				it->set_is_unknown();

                        if (it->is_file()) {
				queuefile(*it);
                        } else {
                                dirheap.push(*it);
			}
		}
	}
};

int main(int argc, char *argv[]) {
	std::string queueType = argv[1];
	std::string root = argv[2];
	struct archive *a;

	a = archive_write_new();

	//archive_write_set_compression_gzip(a);

	archive_write_set_compression_none(a);
	archive_write_set_format_ustar(a);
	archive_write_set_bytes_per_block(a, 128 * 1024);

	archive_write_open_file(a, "/tmp/z.tar");

	if (queueType == "CScan") {
		Traverser<CScan> t(a);

		t.walk(root);
	} else if (queueType == "Queue") {
		Traverser<Queue> t(a);

		t.walk(root);
	}
	archive_write_close(a);
	archive_write_finish(a);
}

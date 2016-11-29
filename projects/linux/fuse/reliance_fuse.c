/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  Copyright (C) 2016       Jean-Christophe Dubois <jcd@tribudubois.net>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 *
 * This file system allows to access a reliance-edge file system on a 
 * block device.
 *
 * Compile with
 *
 *     make
 *
 * Run with
 *
 *     mkdir /tmp/reliance
 *     sudo ./reliance_fuse --device=/dev/ram15 -o auto_unmount -o allow_other /tmp/reliance/
 *
 */

#define FUSE_USE_VERSION 28

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#if defined (st_atime)
#undef st_atime
#undef st_ctime
#undef st_mtime
#define __POSIX_2008_STAT__
#endif

#include <redfs.h>
#include <redtests.h>
#include <redposix.h>
#include <redvolume.h>

static struct options {
	const uint8_t volume_num;
	const char *file_name;
	const bool format;
	int show_help;
} options;

#define OPTION(t, p) { t, offsetof(struct options, p), 1 }

static const struct fuse_opt option_spec[] = {
        OPTION("--vol=%hhd", volume_num),
        OPTION("--device=%s", file_name),
        OPTION("--format", format),
        OPTION("-h", show_help),
        OPTION("--help", show_help),
        FUSE_OPT_END
};

#define path_to_redpath(path, output_str) \
	snprintf(output_str, PATH_MAX, "%s%c%s", \
		gaRedVolConf[options.volume_num].pszPathPrefix, \
		REDCONF_PATH_SEPARATOR, path) 

static mode_t redmode_to_mode(uint16_t redmode)
{
	mode_t linux_mode;

	/*
	   There is no read/write/execute permission on the reliance-edge
	   file system. So we have to generate them.
         */

	if (RED_S_ISDIR(redmode)) {
		linux_mode = S_IFDIR;
		linux_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
	} else {
		linux_mode = S_IFREG;
	}

	/*
	   We always have read access.
	 */
	linux_mode |= S_IRUSR | S_IRGRP | S_IROTH;
#if (REDCONF_READ_ONLY == 0)
	/*
	   If RO, then no write access.
	 */
	linux_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
#endif

	return linux_mode;
}

static int rederrno_to_errno(REDSTATUS rederrno)
{
	switch (rederrno) {
	case 0: return 0;
	case RED_EPERM: return -EPERM;
	case RED_ENOENT: return -ENOENT;
	case RED_EIO: return -EIO;
	case RED_EBADF: return -EBADF;
	case RED_ENOMEM: return -ENOMEM;
	case RED_EBUSY: return -EBUSY;
	case RED_EEXIST: return -EEXIST;
	case RED_EXDEV: return -EXDEV;
	case RED_ENOTDIR: return -ENOTDIR;
	case RED_EISDIR: return -EISDIR;
	case RED_EINVAL: return -EINVAL;
	case RED_ENFILE: return -ENFILE;
	case RED_EMFILE: return -EMFILE;
	case RED_EFBIG: return -EFBIG;
	case RED_ENOSPC: return -ENOSPC;
	case RED_EROFS: return -EROFS;
	case RED_EMLINK: return -EMLINK;
	case RED_ERANGE: return -ERANGE;
	case RED_ENAMETOOLONG: return -ENAMETOOLONG;
	case RED_ENOSYS: return -ENOSYS;
	case RED_ENOTEMPTY: return -ENOTEMPTY;
	case RED_ENODATA: return -ENODATA;
	case RED_EUSERS: return -EUSERS;
	/* it should not happen but default is EINVAL */
	default: return -EINVAL;
	}
}

static uint32_t flags_to_redflags(int flags)
{
	uint32_t red_flags = RED_O_RDONLY;

	if (flags & O_WRONLY) {
		red_flags = RED_O_WRONLY;
	} else if (flags & O_RDWR) {
		red_flags = RED_O_RDWR;
	}
	
	if (flags & O_CREAT) {
		red_flags |= RED_O_CREAT;
	}
	
	if (flags & O_TRUNC) {
		red_flags |= RED_O_TRUNC;
	}
	
	if (flags & O_EXCL) {
		red_flags |= RED_O_EXCL;
	}
	
	if (flags & O_APPEND) {
		red_flags |= RED_O_APPEND;
	}

	return red_flags;
}

static int32_t red_local_open(const char *path, int flags)
{
	char reliance_path[PATH_MAX];

	path_to_redpath(path, reliance_path);

	return red_open(reliance_path, flags_to_redflags(flags));
}

static void *reliance_init(struct fuse_conn_info *conn)
{
	uint32_t iErr;
	const char *pszVolume = gaRedVolConf[options.volume_num].pszPathPrefix;

	(void) conn;

	iErr = red_init();
	if(iErr == -1) {
		fprintf(stderr, "Unexpected error %d from red_init()\n",
			(int)red_errno);
		exit(red_errno);
	}

	iErr = RedOsBDevConfig(options.volume_num, options.file_name);
	if(iErr != 0) {
		fprintf(stderr, "Unexpected error %d from RedOsBDevConfig()\n",
			(int)iErr);
		exit(iErr);
	}
	
	if (options.format) {
#if (REDCONF_API_POSIX_FORMAT == 1)
		iErr = red_format(pszVolume);
		if(iErr == -1) {
			fprintf(stderr, "Unexpected error %d from red_format()\n",
				(int)red_errno);
			exit(red_errno);
		}
#else
		fprintf(stderr, "red_format() is not supported\n");
		exit(-1);
#endif
	}

	iErr = red_mount(pszVolume);
	if(iErr == -1) {
		fprintf(stderr, "Unexpected error %d from red_mount()\n",
			(int)red_errno);
		exit(red_errno);
	}

	return NULL;
}

static int reliance_getattr(const char *path, struct stat *stbuf)
{
	int res;
	int32_t fd;
	REDSTAT redstbuf;

	/* Set the structure to 0 */
	memset(stbuf, 0, sizeof(*stbuf));

	fd = red_local_open(path, O_RDONLY);
	if (fd == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		res = red_fstat(fd, &redstbuf);
		if (res == 0) {
			/* We need to translate the Reliance stat to Unix stat */
			stbuf->st_dev = redstbuf.st_dev;
			stbuf->st_ino = redstbuf.st_ino;
			stbuf->st_mode = redmode_to_mode(redstbuf.st_mode);
			stbuf->st_nlink = redstbuf.st_nlink;
			stbuf->st_size = redstbuf.st_size;
#if (REDCONF_INODE_TIMESTAMPS == 1)
#if defined(__POSIX_2008_STAT__)
			stbuf->st_atim.tv_sec = redstbuf.st_atime;
			stbuf->st_ctim.tv_sec = redstbuf.st_ctime;
			stbuf->st_mtim.tv_sec = redstbuf.st_mtime;
#else
			stbuf->st_atime = redstbuf.st_atime;
			stbuf->st_ctime = redstbuf.st_ctime;
			stbuf->st_mtime = redstbuf.st_mtime;
#endif
#endif
#if (REDCONF_INODE_BLOCKS == 1)
			stbuf->st_blocks = redstbuf.st_blocks;
#endif
		} else {
			res = rederrno_to_errno(red_errno);
		}

		(void)red_close(fd);
	}

	return res;
}

static int reliance_access(const char *path, int mask)
{
	struct stat linux_stat;
	int res = 0;

	res = reliance_getattr(path, &linux_stat);

	if (res == 0) {
		if (mask && ((linux_stat.st_mode & mask) != mask)) {
			res = -EACCES;
		}
	}

	return res;
}

static int reliance_readlink(const char *path, char *buf, size_t size)
{
	(void) path;
	(void) buf;
	(void) size;

	return -ENOSYS;
}


static int reliance_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			    off_t offset, struct fuse_file_info *fi)
{
#if (REDCONF_API_POSIX_READDIR == 1)
	REDDIR *dp;
	REDDIRENT *de;
	int res = 0;
	char reliance_path[PATH_MAX];

	(void) offset;
	(void) fi;

	path_to_redpath(path, reliance_path);

	dp = red_opendir(reliance_path);

	if (dp == NULL) {
		res = rederrno_to_errno(red_errno);
	} else {
		while ((de = red_readdir(dp)) != NULL) {
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = redmode_to_mode(de->d_stat.st_mode);
			if (filler(buf, de->d_name, &st, 0)) {
				break;
			}
		}

		(void)red_closedir(dp);
	}

	return res;
#else
	(void) path;
	(void) filler;
	(void) offset;
	(void) fi;

	return -ENOSYS
#endif
}

static int reliance_mknod(const char *path, mode_t mode, dev_t rdev)
{
#if (REDCONF_READ_ONLY == 1)
	(void) path;
	(void) mode;
	(void) rdev;

	return -ENOSYS
#else
	int res = 0;
	int32_t fd;

	(void) rdev;

	if (S_ISREG(mode)) {
                fd = red_local_open(path+1, O_CREAT | O_EXCL | O_WRONLY);
                if (fd >= 0) {
                        (void)red_close(fd);
		} else {
			res = rederrno_to_errno(red_errno);
		}
        } else {
		res = -ENOSYS;
	}

	return res;
#endif
}

static int reliance_mkdir(const char *path, mode_t mode)
{
#if (REDCONF_API_POSIX_MKDIR == 1)
	int res;
	char reliance_path[PATH_MAX];

	(void) mode;

	path_to_redpath(path, reliance_path);

	res = red_mkdir(reliance_path);

	if (res == -1) {
		res = rederrno_to_errno(red_errno);
	}

	return res;
#else
	(void) path;
	(void) mode;

	return -ENOSYS
#endif
}

static int reliance_unlink(const char *path)
{
#if (REDCONF_API_POSIX_UNLINK == 1)
	int res;
	char reliance_path[PATH_MAX];

	path_to_redpath(path, reliance_path);

	res = red_unlink(reliance_path);

	if (res == -1) {
		res = rederrno_to_errno(red_errno);
	}

	return res;
#else
	(void) path;

	return -ENOSYS
#endif
}

static int reliance_rmdir(const char *path)
{
#if (REDCONF_API_POSIX_RMDIR == 1)
	int res;
	char reliance_path[PATH_MAX];

	path_to_redpath(path, reliance_path);

	res = red_rmdir(reliance_path);

	if (res == -1) {
		res = rederrno_to_errno(red_errno);
	}

	return res;
#else
	(void) path;

	return -ENOSYS
#endif
}

static int reliance_symlink(const char *from, const char *to)
{
	(void) from;
	(void) to;

	/* Not supported on Reliance */

	return -ENOSYS;
}

static int reliance_rename(const char *from, const char *to)
{
#if (REDCONF_API_POSIX_RENAME == 1)
	int res;
	char reliance_path_from[PATH_MAX];
	char reliance_path_to[PATH_MAX];

	path_to_redpath(from, reliance_path_from);

	path_to_redpath(to, reliance_path_to);

	res = red_rename(reliance_path_from, reliance_path_to);

	if (res == -1) {
		res = rederrno_to_errno(red_errno);
	}

	return res;
#else
	(void) from;
	(void) to;

	return -ENOSYS
#endif
}

static int reliance_link(const char *from, const char *to)
{
#if (REDCONF_API_POSIX_LINK == 1)
	int res;
	char reliance_path_from[PATH_MAX];
	char reliance_path_to[PATH_MAX];

	path_to_redpath(from, reliance_path_from);

	path_to_redpath(to, reliance_path_to);

	res = red_link(reliance_path_from, reliance_path_to);

	if (res == -1) {
		res = rederrno_to_errno(red_errno);
	}

	return res;
#else
	(void) from;
	(void) to;

	return -ENOSYS
#endif
}

static int reliance_chmod(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	/* Not supported on Reliance */

	return -ENOSYS;
}

static int reliance_chown(const char *path, uid_t uid, gid_t gid)
{
	(void) path;
	(void) uid;
	(void) gid;

	/* Not supported on Reliance */

	return -ENOSYS;
}

static int reliance_truncate(const char *path, off_t size)
{
#if (REDCONF_API_POSIX_FTRUNCATE == 1)
	int32_t fd;
	int res = 0;

	fd = red_local_open(path, O_WRONLY);

	if (fd == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		res = red_ftruncate(fd, size);
		if (res == -1) {
			res = rederrno_to_errno(red_errno);
		}

		(void)red_close(fd);
	}

	return res;
#else
	(void) path;
	(void) size;

	return -ENOSYS
#endif
}

static int reliance_utimens(const char *path, const struct timespec ts[2])
{
#if (REDCONF_READ_ONLY == 1)
	(void) path;
	(void) ts;

	return -ENOSYS;
#else
	/*
	   This is more of a hack. Reliance does not provide ways to change
	   the date attached to a file.
	   As a workaround, we add a byte at the end, then remove it.
	   As a result, reliance will write the file to the media and
	   change the date to the current date.
	 */
	int32_t fd;
	int res = 0;

	(void) ts;

	fd = red_local_open(path, O_WRONLY);

	if (fd == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		int file_size;

		file_size = red_lseek(fd, 0, RED_SEEK_END);
		if (file_size == -1) {
			res = rederrno_to_errno(red_errno);
		} else {
			char tmp = 0;

			res = red_write(fd, &tmp, sizeof(tmp));
			if (res == -1) {
				res = rederrno_to_errno(red_errno);
			} else {
				res = red_ftruncate(fd, file_size);
				if (res == -1) {
					res = rederrno_to_errno(red_errno);
				}
			}
		}

		(void)red_close(fd);
	}

	return res;
#endif
}

static int reliance_open(const char *path, struct fuse_file_info *fi)
{
	int32_t fd;
	int res = 0;

	fd = red_local_open(path, fi->flags);

	if (fd == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		(void)red_close(fd);
	}

	return res;
}

static int reliance_read(const char *path, char *buf, size_t size, off_t offset,
			 struct fuse_file_info *fi)
{
	int32_t fd;
	int res = 0;

	(void) fi;

	fd = red_local_open(path, O_RDONLY);
	if (fd == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		res = red_lseek(fd, offset, RED_SEEK_SET);
		if (res == -1) {
			res = rederrno_to_errno(red_errno);
		} else {
			res = red_read(fd, buf, size);
			if (res == -1) {
				res = rederrno_to_errno(red_errno);
			}
		}

		(void)red_close(fd);
	}

	return res;
}

static int reliance_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
#if (REDCONF_READ_ONLY == 1)
	(void) path;
	(void) buf;
	(void) size;
	(void) offset;
	(void) fi;

	return -ENOSYS
#else
	int32_t fd;
	int res = 0;

	(void) fi;

	fd = red_local_open(path, O_WRONLY);

	if (fd == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		res = red_lseek(fd, offset, RED_SEEK_SET);
		if (res == -1) {
			res = rederrno_to_errno(red_errno);
		} else {
			res = red_write(fd, buf, size);
			if (res == -1) {
				res = rederrno_to_errno(red_errno);
			}
		}

		(void)red_close(fd);
	}

	return res;
#endif
}

static int reliance_statfs(const char *path, struct statvfs *stbuf)
{
	int res = 0;
	REDSTATFS redstbuf;

	res = red_statvfs(path, &redstbuf);
	if (res == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		stbuf->f_bsize = redstbuf.f_bsize;
		stbuf->f_frsize = redstbuf.f_frsize;
		stbuf->f_blocks = redstbuf.f_blocks;
		stbuf->f_bfree = redstbuf.f_bfree;
		stbuf->f_bavail = redstbuf.f_bavail;
		stbuf->f_files = redstbuf.f_files;
		stbuf->f_ffree = redstbuf.f_ffree;
		stbuf->f_favail = redstbuf.f_favail;
		stbuf->f_fsid = redstbuf.f_fsid;
		stbuf->f_flag = redstbuf.f_flag;
		stbuf->f_namemax = redstbuf.f_namemax;
	}

	return res;
}

static int reliance_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;

	return 0;
}

static int reliance_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
#if (REDCONF_READ_ONLY == 1)
	(void) path;
	(void) isdatasync;
	(void) fi;

	return -ENOSYS
#else
	int32_t fd;
	int res = 0;

	(void) isdatasync;
	(void) fi;

	fd = red_local_open(path, O_WRONLY);

	if (fd == -1) {
		res = rederrno_to_errno(red_errno);
	} else {
		res = red_fsync(fd);
		if (res == -1) {
			res = rederrno_to_errno(red_errno);
		}

		(void)red_close(fd);
	}

	return res;
#endif
}

static struct fuse_operations reliance_oper = {
	.init           = reliance_init,
	.getattr	= reliance_getattr,
	.access		= reliance_access,
	.readlink	= reliance_readlink,
	.readdir	= reliance_readdir,
	.mknod		= reliance_mknod,
	.mkdir		= reliance_mkdir,
	.symlink	= reliance_symlink,
	.unlink		= reliance_unlink,
	.rmdir		= reliance_rmdir,
	.rename		= reliance_rename,
	.link		= reliance_link,
	.chmod		= reliance_chmod,
	.chown		= reliance_chown,
	.truncate	= reliance_truncate,
	.utimens	= reliance_utimens,
	.open		= reliance_open,
	.read		= reliance_read,
	.write		= reliance_write,
	.statfs		= reliance_statfs,
	.release	= reliance_release,
	.fsync		= reliance_fsync,
};

static void show_help(const char *progname)
{
	fprintf(stderr, "usage: %s [options] <mountpoint>\n\n", progname);
	fprintf(stderr, "Reliance specific options:\n"
		"    --vol=<volume_num>\n"
		"    --device=<file_name>\n"
		"    --format\n"
		"\n");
}

int main(int argc, char *argv[], char *argp[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	(void) argp;

	/* Parse options */
        if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	/* When --help is specified, first print our own file-system
	   specific help text, then signal fuse_main to show
	   additional help (by adding `--help` to the options again)
	   without usage: line (by setting argv[0] to the empty
	   string) */
	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0] = (char*) "";
        }

	if (options.file_name == NULL) {
		fprintf(stderr,
			"You need to specify a file name (option --device)"
			" for Reliance\n\n");
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
	}

	return fuse_main(args.argc, args.argv, &reliance_oper, NULL);
}

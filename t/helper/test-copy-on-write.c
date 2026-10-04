#include "test-tool.h"

static int copy_one(const char *source, const char *target, int fail_cache)
{
	struct stat st;
	int src_fd, dst_fd;
	int ret;

	src_fd = open(source, O_RDONLY);
	if (src_fd < 0 || fstat(src_fd, &st))
		return 1;
	if (fail_cache) {
		/* A read-only destination forces a real resize failure. */
		ret = file_copy_on_write(src_fd, src_fd, st.st_size, source);
		close(src_fd);
		return !ret || file_copy_on_write_supported(target);
	}
	dst_fd = open(target, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (dst_fd < 0) {
		close(src_fd);
		return 1;
	}

	ret = file_copy_on_write(dst_fd, src_fd, st.st_size, target);
	if (close(dst_fd))
		ret = -1;
	close(src_fd);
	if (ret)
		unlink(target);
	return !!ret;
}

int cmd__copy_on_write(int argc, const char **argv)
{
	int i;

	if (argc >= 4 && !(argc % 2) && !strcmp(argv[1], "--probe-cwd")) {
		for (i = 2; i < argc; i += 2) {
			if (chdir(argv[i]) ||
			    !!file_copy_on_write_supported("relative-file") !=
			    !!atoi(argv[i + 1]))
				return 1;
		}
		return 0;
	}
	if (argc == 4 && !strcmp(argv[1], "--fail-cache"))
		return copy_one(argv[2], argv[3], 1);
	if (argc >= 4 && !(argc % 2) && !strcmp(argv[1], "--batch")) {
		for (i = 2; i < argc; i += 2)
			if (copy_one(argv[i], argv[i + 1], 0))
				return 1;
		return 0;
	}
	return argc == 3 ? copy_one(argv[1], argv[2], 0) : 1;
}

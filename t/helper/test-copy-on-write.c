#include "test-tool.h"

int cmd__copy_on_write(int argc, const char **argv)
{
	struct stat st;
	int src_fd, dst_fd;
	int ret;

	if (argc != 3)
		return 1;

	src_fd = open(argv[1], O_RDONLY);
	if (src_fd < 0 || fstat(src_fd, &st))
		return 1;
	dst_fd = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (dst_fd < 0) {
		close(src_fd);
		return 1;
	}

	ret = file_copy_on_write(dst_fd, src_fd, st.st_size);
	if (close(dst_fd))
		ret = -1;
	close(src_fd);
	if (ret)
		unlink(argv[2]);
	return !!ret;
}

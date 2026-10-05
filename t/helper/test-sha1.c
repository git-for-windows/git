#define USE_THE_REPOSITORY_VARIABLE
#include "test-tool.h"
#include "hash.h"
#include "setup.h"

int cmd__sha1(int ac, const char **av)
{
	return cmd_hash_impl(ac, av, GIT_HASH_SHA1, 0);
}

int cmd__sha1_is_sha1dc(int argc, const char **argv)
{
#ifdef DC_SHA1_RS
	if (argc == 2 && !strcmp(argv[1], "--backend")) {
		git_SHA_CTX ctx;
		int nongit;

		setup_git_directory_gently(the_repository, &nongit);
		git_SHA1_Init(&ctx);
		puts(sha1dc_init == git_SHA1DCInit ? "c" : "rust");
		git_SHA1_Discard(&ctx);
		return 0;
	}
#else
	if (argc == 2 && !strcmp(argv[1], "--backend"))
		return 1;
#endif
#ifdef platform_SHA_IS_SHA1DC
	return 0;
#endif
	return 1;
}

int cmd__sha1_unsafe(int ac, const char **av)
{
	return cmd_hash_impl(ac, av, GIT_HASH_SHA1, 1);
}

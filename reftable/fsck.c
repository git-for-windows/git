#include "basics.h"
#include "reftable-fsck.h"
#include "stack.h"

static bool valid_table_name(const char *name, uint64_t *min_update_index,
			     uint64_t *max_update_index)
{
	const char *ptr = name;
	char *endptr;

	/* strtoull doesn't set errno on success */
	errno = 0;

	*min_update_index = strtoull(ptr, &endptr, 16);
	if (errno == EINVAL)
		return false;
	ptr = endptr;

	if (strncmp(ptr, "-", 1))
		return false;
	ptr++;

	*max_update_index = strtoull(ptr, &endptr, 16);
	if (errno == EINVAL)
		return false;
	ptr = endptr;

	if (*ptr != '-')
		return false;
	ptr++;

	strtoul(ptr, &endptr, 16);
	if (errno == EINVAL)
		return false;
	ptr = endptr;

	if (strcmp(ptr, ".ref") && strcmp(ptr, ".log"))
		return false;

	return true;
}

static int stack_check_all_files_in_dir(struct reftable_stack *stack,
					reftable_fsck_report_fn report_fn,
					void *cb_data)
{
	DIR *dir = opendir(stack->reftable_dir);
	struct reftable_fsck_info info;
	struct dirent *d = NULL;
	uint64_t min, max;
	int err = 0;

	if (!dir)
		return 0;

	while ((d = readdir(dir))) {
		if (!strcmp(d->d_name, "tables.list"))
			continue;

		if ((d->d_name[0] == '.' &&
		     (d->d_name[1] == '\0' ||
		      (d->d_name[1] == '.' && d->d_name[2] == '\0'))))
			continue;

		if (d->d_type == DT_REG) {
			if (!valid_table_name(d->d_name, &min, &max)) {
				info.error = REFTABLE_FSCK_ERROR_TABLE_NAME;
				info.msg = "file with invalid table name";
				info.path = d->d_name;

				err |= report_fn(&info, cb_data);
			}
		} else {
			info.error = REFTABLE_FSCK_ERROR_INVALID_FILE_TYPE;
			info.msg = "file with unexpected type";
			info.path = d->d_name;

			err |= report_fn(&info, cb_data);
		}
	}

	closedir(dir);
	return err;
}

static int stack_checks(struct reftable_stack *stack,
			reftable_fsck_report_fn report_fn,
			void *cb_data)
{
	struct reftable_buf msg = REFTABLE_BUF_INIT;
	char **names = NULL;
	int err = 0;

	if (stack == NULL)
		goto out;

	err |= stack_check_all_files_in_dir(stack, report_fn, cb_data);

out:
	free_names(names);
	reftable_buf_release(&msg);
	return err;
}

int reftable_fsck_check(struct reftable_stack *stack,
			reftable_fsck_report_fn report_fn,
			reftable_fsck_verbose_fn verbose_fn,
			void *cb_data)
{
	verbose_fn("Checking reftable: stack checks", cb_data);
	return stack_checks(stack, report_fn, cb_data);
}

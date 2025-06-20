#define USE_THE_REPOSITORY_VARIABLE

#include "builtin.h"
#include "environment.h"
#include "hash.h"
#include "json-writer.h"
#include "parse-options.h"
#include "quote.h"
#include "refs.h"
#include "shallow.h"

enum output_format {
	FORMAT_JSON,
	FORMAT_PLAINTEXT
};

enum repo_info_category {
	CATEGORY_REFERENCES = 1 << 0,
	CATEGORY_LAYOUT = 1 << 1
};

enum repo_info_references_field {
	FIELD_REFERENCES_FORMAT = 1 << 0
};

enum repo_info_layout_field {
	FIELD_LAYOUT_BARE = 1 << 0,
	FIELD_LAYOUT_SHALLOW = 1 << 1
};

struct repo_info_field {
	enum repo_info_category category;
	union {
		enum repo_info_references_field references;
		enum repo_info_layout_field layout;
	} field;
};

struct repo_info {
	struct repository *repo;
	enum output_format format;
	int n_fields;
	struct repo_info_field *fields;
};

static struct repo_info_field default_fields[] = {
	{
		.category = CATEGORY_REFERENCES,
		.field.references = FIELD_REFERENCES_FORMAT
	},
	{
		.category = CATEGORY_LAYOUT,
		.field.layout = FIELD_LAYOUT_BARE
	},
	{
		.category = CATEGORY_LAYOUT,
		.field.layout = FIELD_LAYOUT_SHALLOW
	}
};

static void print_key_value(const char *key, const char *value) {
	printf("%s=", key);
	quote_c_style(value, NULL, stdout, 0);
	putchar('\n');
}

static void repo_info_init(struct repo_info *repo_info,
			   struct repository *repo,
			   char *format,
			   int allow_empty,
			   int argc, const char **argv)
{
	int i;
	repo_info->repo = repo;

	if (format == NULL || !strcmp(format, "json"))
		repo_info->format = FORMAT_JSON;
	else if (!strcmp(format, "plaintext"))
		repo_info->format = FORMAT_PLAINTEXT;
	else
		die("invalid format %s", format);

	if (argc == 0 && !allow_empty) {
		repo_info->n_fields = ARRAY_SIZE(default_fields);
		repo_info->fields = default_fields;
	} else {
		repo_info->n_fields = argc;
		ALLOC_ARRAY(repo_info->fields, argc);

		for (i = 0; i < argc; i++) {
			const char *arg = argv[i];
			struct repo_info_field *field = repo_info->fields + i;

			if (!strcmp(arg, "references.format")) {
				field->category = CATEGORY_REFERENCES;
				field->field.references = FIELD_REFERENCES_FORMAT;
			} else if (!strcmp(arg, "layout.bare")) {
				field->category = CATEGORY_LAYOUT;
				field->field.layout = FIELD_LAYOUT_BARE;
			} else if (!strcmp(arg, "layout.shallow")) {
				field->category = CATEGORY_LAYOUT;
				field->field.layout = FIELD_LAYOUT_SHALLOW;
			} else {
				die("invalid field '%s'", arg);
			}
		}
	}
}

static void repo_info_release(struct repo_info *repo_info)
{
	if (repo_info->fields != default_fields) free(repo_info->fields);
}

static void repo_info_print_plaintext(struct repo_info *repo_info) {
	struct repository *repo = repo_info->repo;
	int i;
	for (i = 0; i < repo_info->n_fields; i++) {
		struct repo_info_field *field = &repo_info->fields[i];
		switch (field->category) {
		case CATEGORY_REFERENCES:
			switch (field->field.references) {
			case FIELD_REFERENCES_FORMAT:
				print_key_value("references.format",
						ref_storage_format_to_name(
							repo->ref_storage_format));
				break;
			}
			break;
		case CATEGORY_LAYOUT:
			switch (field->field.layout) {
			case FIELD_LAYOUT_BARE:
				print_key_value("layout.bare",
						is_bare_repository() ?
							"true" : "false");
				break;
			case FIELD_LAYOUT_SHALLOW:
				print_key_value("layout.shallow",
						is_repository_shallow(repo) ?
							"true" : "false");
				break;
			}
			break;
		}
	}
}

static void repo_info_print_json(struct repo_info *repo_info)
{
	struct json_writer jw;
	int i;
	unsigned int categories = 0;
	unsigned int references_fields = 0;
	unsigned int layout_fields = 0;
	struct repository *repo = repo_info->repo;

	for (i = 0; i < repo_info->n_fields; i++) {
		struct repo_info_field *field = repo_info->fields + i;
		categories |= field->category;
		switch (field->category) {
		case CATEGORY_REFERENCES:
			references_fields |= field->field.references;
			break;
		case CATEGORY_LAYOUT:
			layout_fields |= field->field.layout;
			break;
		}
	}

	jw_init(&jw);

	jw_object_begin(&jw, 1);

	if (categories & CATEGORY_REFERENCES) {
		jw_object_inline_begin_object(&jw, "references");
		if (references_fields & FIELD_REFERENCES_FORMAT) {
			const char *format_name = ref_storage_format_to_name(
				repo->ref_storage_format);
			jw_object_string(&jw, "format", format_name);
		}
		jw_end(&jw);
	}

	if (categories & CATEGORY_LAYOUT) {
		jw_object_inline_begin_object(&jw, "layout");
		if (layout_fields & FIELD_LAYOUT_BARE) {
			jw_object_bool(&jw, "bare",
				       is_bare_repository());
		}

		if (layout_fields & FIELD_LAYOUT_SHALLOW) {
			jw_object_bool(&jw, "shallow",
				       is_repository_shallow(repo));
		}
		jw_end(&jw);
	}
	jw_end(&jw);

	puts(jw.json.buf);
	jw_release(&jw);
}

static void repo_info_print(struct repo_info *repo_info)
{
	enum output_format format = repo_info->format;

	switch (format) {
	case FORMAT_JSON:
		repo_info_print_json(repo_info);
		break;
	case FORMAT_PLAINTEXT:
		repo_info_print_plaintext(repo_info);
		break;
	}
}

int cmd_repo_info(int argc,
		  const char **argv,
		  const char *prefix,
		  struct repository *repo)
{
	const char *const repo_info_usage[] = {
		"git repo-info",
		NULL
	};
	struct repo_info repo_info;
	char *format = NULL;
	int allow_empty = 0;
	struct option options[] = {
		OPT_STRING(0, "format", &format, N_("format"),
			   N_("output format")),
		OPT_BOOL(0, "allow-empty", &allow_empty,
			 "when set, it will use an empty set of fields if no field is requested"),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, repo_info_usage,
			     PARSE_OPT_KEEP_UNKNOWN_OPT);
	repo_info_init(&repo_info, repo, format, allow_empty, argc, argv);
	repo_info_print(&repo_info);
	repo_info_release(&repo_info);

	return 0;
}

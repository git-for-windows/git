#include "git-compat-util.h"
#include "hex.h"
#include "json-parser.h"

int reset_iterator(struct json_iterator *it)
{
	it->p = it->begin = it->json;
	strbuf_release(&it->key);
	strbuf_release(&it->string_value);
	it->type = JSON_NULL;
	return -1;
}

static int parse_json_string(struct json_iterator *it, struct strbuf *out)
{
	const char *begin = it->p;

	if (*(it->p)++ != '"') {
		error("expected double quote: '%.*s'", 5, begin);
		return reset_iterator(it);
	}

	strbuf_reset(&it->string_value);
#define APPEND(c) strbuf_addch(out, c)
	while (*it->p != '"') {
		switch (*it->p) {
		case '\0':
			error("incomplete string: '%s'", begin);
			return reset_iterator(it);
		case '\\':
			it->p++;
			if (*it->p == '\\' || *it->p == '"')
				APPEND(*it->p);
			else if (*it->p == 'b')
				APPEND(8);
			else if (*it->p == 't')
				APPEND(9);
			else if (*it->p == 'n')
				APPEND(10);
			else if (*it->p == 'f')
				APPEND(12);
			else if (*it->p == 'r')
				APPEND(13);
			else if (*it->p == 'u') {
				unsigned char binary[2];
				int i;

				if (hex_to_bytes(binary, it->p + 1, 2) < 0) {
					error("invalid: '%.*s'", 6, it->p - 1);
					return reset_iterator(it);
				}
				it->p += 4;

				i = (binary[0] << 8) | binary[1];
				if (i < 0x80)
					APPEND(i);
				else if (i < 0x0800) {
					APPEND(0xc0 | ((i >> 6) & 0x1f));
					APPEND(0x80 | (i & 0x3f));
				} else if (i < 0x10000) {
					APPEND(0xe0 | ((i >> 12) & 0x0f));
					APPEND(0x80 | ((i >> 6) & 0x3f));
					APPEND(0x80 | (i & 0x3f));
				} else {
					APPEND(0xf0 | ((i >> 18) & 0x07));
					APPEND(0x80 | ((i >> 12) & 0x3f));
					APPEND(0x80 | ((i >> 6) & 0x3f));
					APPEND(0x80 | (i & 0x3f));
				}
			}
			break;
		default:
			APPEND(*it->p);
		}
		it->p++;
	}

	it->end = it->p++;
	return 0;
}

static void skip_whitespace(struct json_iterator *it)
{
	while (isspace(*it->p))
		it->p++;
}

int iterate_json(struct json_iterator *it)
{
	skip_whitespace(it);
	it->begin = it->p;

	switch (*it->p) {
	case '\0':
		reset_iterator(it);
		return 0;
	case 'n':
		if (!starts_with(it->p, "null")) {
			error("unexpected value: %.*s", 4, it->p);
			return reset_iterator(it);
		}
		it->type = JSON_NULL;
		it->end = it->p = it->begin + 4;
		break;
	case 't':
		if (!starts_with(it->p, "true")) {
			error("unexpected value: %.*s", 4, it->p);
			return reset_iterator(it);
		}
		it->type = JSON_TRUE;
		it->end = it->p = it->begin + 4;
		break;
	case 'f':
		if (!starts_with(it->p, "false")) {
			error("unexpected value: %.*s", 5, it->p);
			return reset_iterator(it);
		}
		it->type = JSON_FALSE;
		it->end = it->p = it->begin + 5;
		break;
	case '-': case '.':
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		it->type = JSON_NUMBER;
		it->end = it->p = it->begin + strspn(it->p, "-.0123456789");
		break;
	case '"':
		it->type = JSON_STRING;
		if (parse_json_string(it, &it->string_value) < 0)
			return -1;
		break;
	case '[': {
		const char *save = it->begin;
		size_t key_offset = it->key.len;
		int i = 0, res;

		for (it->p++, skip_whitespace(it); *it->p != ']'; i++) {
			strbuf_addf(&it->key, "[%d]", i);

			if ((res = iterate_json(it))) {
				reset_iterator(it);
				return res;
			}
			strbuf_setlen(&it->key, key_offset);

			skip_whitespace(it);
			if (*it->p == ',')
				it->p++;
		}

		it->type = JSON_ARRAY;
		it->begin = save;
		it->end = it->p;
		it->p++;
		break;
	}
	case '{': {
		const char *save = it->begin;
		size_t key_offset = it->key.len;
		int res;

		strbuf_addch(&it->key, '.');
		for (it->p++, skip_whitespace(it); *it->p != '}'; ) {
			strbuf_setlen(&it->key, key_offset + 1);
			if (parse_json_string(it, &it->key) < 0)
				return -1;
			skip_whitespace(it);
			if (*(it->p)++ != ':') {
				error("expected colon: %.*s", 5, it->p);
				return reset_iterator(it);
			}

			if ((res = iterate_json(it)))
				return res;

			skip_whitespace(it);
			if (*it->p == ',')
				it->p++;
		}
		strbuf_setlen(&it->key, key_offset);

		it->type = JSON_OBJECT;
		it->begin = save;
		it->end = it->p;
		it->p++;
		break;
	}
	}

	return it->fn(it);
}

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "strbuf.h"

struct json_iterator {
	const char *json, *p, *begin, *end;
	struct strbuf key, string_value;
	enum {
		JSON_NULL = 0,
		JSON_FALSE,
		JSON_TRUE,
		JSON_NUMBER,
		JSON_STRING,
		JSON_ARRAY,
		JSON_OBJECT
	} type;
	int (*fn)(struct json_iterator *it);
	void *fn_data;
};
#define JSON_ITERATOR_INIT(json_, fn_, fn_data_) { \
	.json = json_, .p = json_, \
	.key = STRBUF_INIT, .string_value = STRBUF_INIT, \
	.fn = fn_, .fn_data = fn_data_ \
}

int iterate_json(struct json_iterator *it);
/* Releases the iterator, always returns -1 */
int reset_iterator(struct json_iterator *it);

#endif

#include "ebucket.hpp"

#include <stdio.h>

using namespace ioremap;

extern "C" {
#include "_cgo_export.h"

ebucket_wrapper *new_ebucket_wrapper(ell_node *node, const uint32_t *groups, int group_num, const char **keys, int key_num)
{
	ebucket_wrapper *bp;
	try {
		bp = new ebucket_wrapper(node->node());

		std::vector<std::string> k;
		for (int i = 0; i < key_num; ++i)
			k.push_back(keys[i]);

		std::vector<int> gr;
		for (int i = 0; i < group_num; ++i)
			gr.push_back(groups[i]);

		try {
			if (!bp->init(gr, k)) {
				delete bp;
				return NULL;
			}

			return bp;
		} catch (const std::exception &e) {
			fprintf(stderr, "could not init bucket processor: %s\n", e.what());
			delete bp;
			return NULL;
		}
	} catch (const std::exception &e) {
		fprintf(stderr, "could not create bucket processor: %s\n", e.what());
		return NULL;
	}
}

ebucket_wrapper *new_ebucket_wrapper_key(ell_node *node, const uint32_t *groups, int group_num, const char *key)
{
	ebucket_wrapper *bp;
	try {
		bp = new ebucket_wrapper(node->node());

		std::vector<int> gr;
		for (int i = 0; i < group_num; ++i)
			gr.push_back(groups[i]);

		try {
			if (!bp->init(gr, key)) {
				delete bp;
				return NULL;
			}

			return bp;
		} catch (const std::exception &e) {
			fprintf(stderr, "could not init bucket processor: %s\n", e.what());
			delete bp;
			return NULL;
		}
	} catch (const std::exception &e) {
		fprintf(stderr, "could not create bucket processor: %s\n", e.what());
		return NULL;
	}
}

void delete_ebucket_wrapper(ebucket_wrapper *ew)
{
	delete ew;
}

int ebucket_get_bucket(ebucket_wrapper *bp, size_t size, bucket_meta *meta)
{
	ebucket::bucket b;
	auto err = bp->get_bucket(size, b);
	if (err) {
		return err.code();
	}

	*meta = b->meta();
	return 0;
}

int ebucket_find_bucket(ebucket_wrapper *bp, const char *name, bucket_meta *meta)
{
	ebucket::bucket b;
	auto err = bp->find_bucket(name, b);
	if (err) {
		return err.code();
	}

	*meta = b->meta();
	return 0;
}

const char *bucket_meta_name(bucket_meta *meta)
{
	return meta->name.c_str();
}
size_t bucket_meta_name_size(bucket_meta *meta)
{
	return meta->name.size();
}

void bucket_meta_groups_copy(void *dst, bucket_meta *meta)
{
	memcpy(dst, meta->groups.data(), meta->groups.size() * sizeof(uint32_t));
}
size_t bucket_meta_groups_size(bucket_meta *meta)
{
	return meta->groups.size();
}

} // extern "C"

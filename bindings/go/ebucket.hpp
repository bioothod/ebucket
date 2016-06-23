#pragma once

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
#include "ebucket/bucket_processor.hpp"
#include "node.h"

typedef ioremap::ebucket::bucket_processor ebucket_wrapper;
typedef ioremap::ebucket::bucket_meta bucket_meta;

extern "C" {
#else
typedef void ebucket_wrapper;
typedef void ell_node;
typedef void bucket_meta;
#endif

ebucket_wrapper *new_ebucket_wrapper(ell_node *node, const uint32_t *groups, int group_num, const char **keys, int key_num);
ebucket_wrapper *new_ebucket_wrapper_key(ell_node *node, const uint32_t *groups, int group_num, const char *key);
void delete_ebucket_wrapper(ebucket_wrapper *ew);

int ebucket_get_bucket(ebucket_wrapper *bp, size_t size, bucket_meta *meta);
int ebucket_find_bucket(ebucket_wrapper *bp, const char *name, bucket_meta *meta);

const char *bucket_meta_name(bucket_meta *meta);
size_t bucket_meta_name_size(bucket_meta *meta);
void bucket_meta_groups_copy(void *dst, bucket_meta *meta);
size_t bucket_meta_groups_size(bucket_meta *meta);

#ifdef __cplusplus
}
#endif

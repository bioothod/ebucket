#ifndef __EBUCKET_CORE_HPP
#define __EBUCKET_CORE_HPP

#include <string>
#include <msgpack.hpp>

#include <blackhole/blackhole.hpp>

namespace ioremap { namespace ebucket {

#define EBUCKET_LOG_ERROR blackhole::defaults::severity::error
#define EBUCKET_LOG_WARNING blackhole::defaults::severity::warning
#define EBUCKET_LOG_INFO blackhole::defaults::severity::info
#define EBUCKET_LOG_NOTICE blackhole::defaults::severity::notice
#define EBUCKET_LOG_DEBUG blackhole::defaults::severity::debug

struct eurl {
	std::string bucket;
	std::string key;

	MSGPACK_DEFINE(bucket, key);


	size_t size() const {
		return bucket.size() + key.size();
	}

	std::string str() const {
		return bucket + "/" + key;
	}

	bool operator!=(const eurl &other) const {
		return bucket != other.bucket || key != other.key;
	}

	bool operator==(const eurl &other) const {
		return bucket == other.bucket && key == other.key;
	}

	bool operator<(const eurl &other) const {
		return bucket < other.bucket || key < other.key;
	}
	bool operator<=(const eurl &other) const {
		return bucket <= other.bucket || key <= other.key;
	}

	bool empty() const {
		return key.empty();
	}
};

}} // namespace ioremap::ebucket

#endif // __EBUCKET_CORE_HPP

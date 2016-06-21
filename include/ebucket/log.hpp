#pragma once

#include <swarm/logger.hpp>

namespace ioremap { namespace nulla {

static inline void dolog(const swarm::logger &logger, swarm::log_level level, const char *fmt, ...) __attribute__ ((format(printf, 3, 4)));

static inline void dolog(const swarm::logger &logger, swarm::log_level level, const char *fmt, ...)
{
	auto record = logger.open_record(level);
	if (record) {
		va_list args;
		va_start(args, fmt);

		char buffer[2048];
		const size_t buffer_size = sizeof(buffer);

		vsnprintf(buffer, buffer_size, fmt, args);

		buffer[buffer_size - 1] = '\0';

		size_t len = strlen(buffer);
		while (len > 0 && buffer[len - 1] == '\n')
			buffer[--len] = '\0';

		try {
			record.attributes.insert(blackhole::keyword::message() = buffer);
		} catch (...) {
		}
		logger.push(std::move(record));
		va_end(args);
	}
}

#define EBUCKET_LOG(level, fmt, a...) ioremap::nulla::dolog(this->logger(), level, fmt, ##a)
#define EBUCKET_LOG_ERROR(fmt, a...) EBUCKET_LOG(SWARM_LOG_ERROR, fmt, ##a)
#define EBUCKET_LOG_WARNING(fmt, a...) EBUCKET_LOG(SWARM_LOG_WARNING, fmt, ##a)
#define EBUCKET_LOG_INFO(fmt, a...) EBUCKET_LOG(SWARM_LOG_INFO, fmt, ##a)
#define EBUCKET_LOG_NOTICE(fmt, a...) EBUCKET_LOG(SWARM_LOG_NOTICE, fmt, ##a)
#define EBUCKET_LOG_DEBUG(fmt, a...) EBUCKET_LOG(SWARM_LOG_DEBUG, fmt, ##a)

}} // namespace ioremap::nulla

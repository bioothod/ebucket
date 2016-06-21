#include "ebucket/bucket_processor.hpp"
#include "ebucket/log.hpp"

#include "elliptics/session.hpp"

#include <thevoid/rapidjson/stringbuffer.h>
#include <thevoid/rapidjson/prettywriter.h>
#include <thevoid/rapidjson/document.h>

#include <thevoid/server.hpp>
#include <thevoid/stream.hpp>

#include <unistd.h>
#include <signal.h>

#include <atomic>

using namespace ioremap;

class JsonValue : public rapidjson::Value
{
public:
	JsonValue() {
		SetObject();
	}

	~JsonValue() {
	}

	static void set_time(rapidjson::Value &obj, rapidjson::Document::AllocatorType &alloc, long tsec, long usec) {
		char str[64];
		struct tm tm;

		localtime_r((time_t *)&tsec, &tm);
		strftime(str, sizeof(str), "%F %Z %R:%S", &tm);

		char time_str[128];
		snprintf(time_str, sizeof(time_str), "%s.%06lu", str, usec);

		obj.SetObject();

		rapidjson::Value tobj(time_str, strlen(time_str), alloc);
		obj.AddMember("time", tobj, alloc);

		std::string raw_time = elliptics::lexical_cast(tsec) + "." + elliptics::lexical_cast(usec);
		rapidjson::Value tobj_raw(raw_time.c_str(), raw_time.size(), alloc);
		obj.AddMember("time-raw", tobj_raw, alloc);
	}

	std::string ToString() const {
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		Accept(writer);
		buffer.Put('\n');

		return std::string(buffer.GetString(), buffer.Size());
	}

	rapidjson::MemoryPoolAllocator<> &GetAllocator() {
		return m_allocator;
	}

private:
	rapidjson::MemoryPoolAllocator<> m_allocator;
};

template <typename Server, typename Stream>
class on_bucket_base : public thevoid::simple_request_stream<Server>, public std::enable_shared_from_this<Stream> {
public:
	virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
		(void) buffer;

		size_t size = 1024;

		try {
			const auto &query = req.url().query();
			size = query.item_value("size", 1024ul);
		} catch (const std::exception &e) {
			EBUCKET_LOG_ERROR("on_request: url: %s: invalid size parameter: %s",
					req.url().to_human_readable().c_str(), e.what());
			this->send_reply(swarm::http_response::bad_request);
			return;
		}

		ebucket::bucket b;
		auto err = this->server()->bucket_processor()->get_bucket(size, b);
		if (err) {
			EBUCKET_LOG_ERROR("on_request: url: %s: could not find bucket for size: %ld, error: %s [%d]",
					req.url().to_human_readable().c_str(), size, err.message().c_str(), err.code());
			this->send_reply(swarm::http_response::service_unavailable);
			return;
		}

		auto meta = b->meta();

		EBUCKET_LOG_INFO("on_request: url: %s: size: %ld, bucket: %s",
			req.url().to_human_readable().c_str(), size, meta.to_string().c_str());

		JsonValue ret;
		auto &allocator = ret.GetAllocator();

		rapidjson::Value name_val(meta.name.c_str(), meta.name.size(), allocator);
		ret.AddMember("bucket", name_val, allocator);

		rapidjson::Value groups_val(rapidjson::kArrayType);
		for (auto group: meta.groups) {
			groups_val.PushBack(group, allocator);
		}
		ret.AddMember("groups", groups_val, allocator);

		std::string data = ret.ToString();

		thevoid::http_response reply;
		reply.set_code(swarm::http_response::ok);
		reply.headers().set_content_type("text/json; charset=utf-8");
		reply.headers().set_content_length(data.size());

		this->send_reply(std::move(reply), std::move(data));
	}

	virtual void on_error(const boost::system::error_code &error) {
		EBUCKET_LOG_ERROR("buffered-read: on_error: url: %s, error: %s",
			this->request().url().to_human_readable().c_str(), error.message().c_str());
	}

private:
};

template <typename Server>
class on_bucket : public on_bucket_base<Server, on_bucket<Server>>
{
public:
};


class ebucket_server : public thevoid::server<ebucket_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		srand(time(NULL));

		if (!elliptics_init(config))
			return false;

		on<on_bucket<ebucket_server>>(
			options::prefix_match("/bucket"),
			options::methods("GET")
		);

		return true;
	}

	std::shared_ptr<ebucket::bucket_processor> bucket_processor() const {
		return m_bp;
	}

private:
	std::shared_ptr<elliptics::node> m_node;
	std::shared_ptr<ebucket::bucket_processor> m_bp;

	long m_read_timeout = 60;
	long m_write_timeout = 60;

	bool elliptics_init(const rapidjson::Value &config) {
		dnet_config node_config;
		memset(&node_config, 0, sizeof(node_config));

		if (!prepare_config(config, node_config)) {
			return false;
		}

		if (!prepare_server(config)) {
			return false;
		}

		m_node.reset(new elliptics::node(std::move(swarm::logger(this->logger(), blackhole::log::attributes_t())), node_config));

		if (!prepare_node(config, *m_node)) {
			return false;
		}

		m_bp.reset(new ebucket::bucket_processor(m_node));

		if (!prepare_session(config)) {
			return false;
		}

		if (!prepare_buckets(config)) {
			return false;
		}

		return true;
	}

	bool prepare_config(const rapidjson::Value &config, dnet_config &node_config) {
		if (config.HasMember("io-thread-num")) {
			node_config.io_thread_num = config["io-thread-num"].GetInt();
		}
		if (config.HasMember("nonblocking-io-thread-num")) {
			node_config.nonblocking_io_thread_num = config["nonblocking-io-thread-num"].GetInt();
		}
		if (config.HasMember("net-thread-num")) {
			node_config.net_thread_num = config["net-thread-num"].GetInt();
		}

		return true;
	}

	bool prepare_node(const rapidjson::Value &config, elliptics::node &node) {
		if (!config.HasMember("remotes")) {
			EBUCKET_LOG_ERROR("\"application.remotes\" field is missed");
			return false;
		}

		std::vector<elliptics::address> remotes;

		auto &remotesArray = config["remotes"];
		for (auto it = remotesArray.Begin(), end = remotesArray.End(); it != end; ++it) {
				if (it->IsString()) {
					remotes.push_back(it->GetString());
				}
		}

		try {
			node.add_remote(remotes);
			elliptics::session s(node);

			if (!s.get_routes().size()) {
				EBUCKET_LOG_ERROR("Didn't add any remote node, exiting.");
				return false;
			}
		} catch (const std::exception &e) {
			EBUCKET_LOG_ERROR("Could not add any out of %ld nodes.", remotes.size());
			return false;
		}

		return true;
	}

	bool prepare_session(const rapidjson::Value &config) {
		if (config.HasMember("read-timeout")) {
			auto &tm = config["read-timeout"];
			if (tm.IsInt())
				m_read_timeout = tm.GetInt();
		}

		if (config.HasMember("write-timeout")) {
			auto &tm = config["write-timeout"];
			if (tm.IsInt())
				m_write_timeout = tm.GetInt();
		}

		return true;
	}

	bool prepare_buckets(const rapidjson::Value &config) {
		if (!config.HasMember("metadata_groups")) {
			EBUCKET_LOG_ERROR("\"application.metadata_groups\" field is missed");
			return false;
		}

		std::vector<int> mgroups;
		auto &groups_meta_array = config["metadata_groups"];
		for (auto it = groups_meta_array.Begin(), end = groups_meta_array.End(); it != end; ++it) {
			if (it->IsInt())
				mgroups.push_back(it->GetInt());
		}

		if (!config.HasMember("buckets") && !config.HasMember("buckets_key")) {
			EBUCKET_LOG_ERROR("neither \"application.buckets\" nor \"application.bucket_key\" fields is present");
			return false;
		}

		if (config.HasMember("buckets")) {
			auto &buckets = config["buckets"];

			std::set<std::string> bnames;
			for (auto it = buckets.Begin(), end = buckets.End(); it != end; ++it) {
				if (it->IsString()) {
					bnames.insert(it->GetString());
				}
			}

			if (!m_bp->init(mgroups, std::vector<std::string>(bnames.begin(), bnames.end())))
				return false;
		} else if (config.HasMember("buckets_key")) {
			const char *bkey = ebucket::get_string(config, "buckets_key");

			if (!m_bp->init(mgroups, bkey))
				return false;
		} else {
			return false;
		}


		return true;
	}

	bool prepare_server(const rapidjson::Value &config) {
		(void) config;
		return true;
	}
};

int main(int argc, char **argv)
{
	if (argc == 1) {
		std::cerr << "Usage: " << argv[0] << " --config <config file>" << std::endl;
		return -1;
	}

	thevoid::register_signal_handler(SIGINT, thevoid::handle_stop_signal);
	thevoid::register_signal_handler(SIGTERM, thevoid::handle_stop_signal);
	thevoid::register_signal_handler(SIGHUP, thevoid::handle_reload_signal);
	thevoid::register_signal_handler(SIGUSR1, thevoid::handle_ignore_signal);
	thevoid::register_signal_handler(SIGUSR2, thevoid::handle_ignore_signal);

	thevoid::run_signal_thread();

	auto server = thevoid::create_server<ebucket_server>();
	int err = server->run(argc, argv);

	thevoid::stop_signal_thread();

	return err;
}


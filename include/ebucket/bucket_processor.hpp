#ifndef __EBUCKET_BUCKET_PROCESSOR_HPP
#define __EBUCKET_BUCKET_PROCESSOR_HPP

#include "ebucket/bucket.hpp"
#include "ebucket/elliptics_stat.hpp"

#include <elliptics/session.hpp>

#include <chrono>
#include <functional>
#include <thread>

namespace ioremap { namespace ebucket {

// This class implements main distribution logic.
// There are multiple @bucket objects in the processor,
// each bucket corresponds to logical entity which handles replication
// and optionally additional internal write load balancing.
//
// Basically saying, @bucket is an entity which holds your data and
// checks whether it is good or not.
//
// Thus, after you data has been written into given bucket, you can only
// read it from that bucket and update it only in that bucket. If you write
// data with the same key into different bucket, that will be a completely different object,
// objects with the same name in different buckets will not be related in any way.
//
// When you do not have (do not even know) about which bucket to use, there is a helper method @get_bucket()
// It will select bucket which suits your needs better than others. Selection logic taks into consideration
// space measurements and various performance metrics (implicit disk and network for instance).
// Selection is probabilistic in nature, thus it is possible that you will not get the best match,
// this probability obeys normal distribution law and decays quickly.
//
// It is only possible to read data from some bucket (where you had written it), not from the storage.
// There is internal per-bucket load balancing for read requests - elliptics maintains internal states
// for all connections and groups (replicas within bucket) and their weights, which correspond to how
// quickly one can read object from given destination point. When reading data from bucket, it selects
// the fastest desistnation point among its replicas and reads data from that point. If data is not available,
// elliptics will automatically fetch (data recover) data from other copies.
class bucket_processor {
public:
	bucket_processor(std::shared_ptr<elliptics::node> &node) :
	m_node(node),
	m_stat(node),
	m_error_session(*m_node),
	m_buckets_update(std::bind(&bucket_processor::buckets_update, this))
	{
		m_error_session.set_exceptions_policy(elliptics::session::no_exceptions);
		m_error_session.set_filter(elliptics::filters::all_with_ack);
	}

	virtual ~bucket_processor() {
		m_need_exit = true;
		m_wait.notify_all();
		if (m_buckets_update.joinable())
			m_buckets_update.join();
	}

	bool init(const std::vector<int> &mgroups, const std::string &bucket_key) {
		if (bucket_key.empty())
			return false;

		std::unique_lock<std::mutex> lock(m_lock);
		m_bucket_key = bucket_key;
		m_meta_groups = mgroups;
		lock.unlock();

		if (!m_bucket_key.empty()) {
			auto err = request_bucket_list(m_bucket_key, true);
			if (err)
				return false;

			init(mgroups, m_bnames);
		}

		return true;
	}

	bool init(const std::vector<int> &mgroups, const std::vector<std::string> &bnames) {
		std::map<std::string, bucket> buckets = read_buckets(mgroups, bnames);

		std::unique_lock<std::mutex> lock(m_lock);

		m_buckets = buckets;
		m_bnames = bnames;
		m_meta_groups = mgroups;

		if (buckets.empty())
			return false;

		return true;
	}

	const elliptics::logger &logger() const {
		return m_node->get_log();
	}

	elliptics::session error_session() const {
		return m_error_session;
	}

	// returns bucket name in @data or negative error code in @error
	elliptics::error_info get_bucket(size_t size, bucket &ret) {
		elliptics::logger &log = m_node->get_log();

		std::unique_lock<std::mutex> guard(m_lock);
		if (m_buckets.size() == 0) {
			return elliptics::create_error(-ENODEV, "there are no buckets at all");
		}

		struct bw {
			bucket		b;
			float		w = 0;
		};

		std::vector<bw> good_buckets;
		good_buckets.reserve(m_buckets.size());

		limits l;
		for (auto it = m_buckets.begin(), end = m_buckets.end(); it != end; ++it) {
			if (!it->second->valid())
				continue;

			bw b;
			b.b = it->second;
			// weight calculation is a rather heavy task, cache this value
			b.w = it->second->weight(size, l);
			if (b.w == 0)
				continue;

			good_buckets.push_back(b);
		}

		guard.unlock();

		if (good_buckets.size() == 0) {
			return elliptics::create_error(-ENODEV, "there are buckets, but they are not suitable for size %zd", size);
		}

		auto routes = m_error_session.get_routes();

		float sum = 0;
		for (auto it = good_buckets.rbegin(), end = good_buckets.rend(); it != end; ++it) {
			// check whether all groups from given buckets are present in the current route table
			size_t have_routes = 0;
			bucket_meta bmeta = it->b->meta();

			for (auto it = routes.begin(), it_end = routes.end(); it != it_end; ++it) {
				for (size_t i = 0; i < bmeta.groups.size(); ++i) {
					if (it->group_id == bmeta.groups[i]) {
						have_routes++;
						break;
					}
				}

				if (have_routes == bmeta.groups.size())
					break;
			}


			// there are no routes to one or more groups in this bucket, heavily decrease its weight
			if (have_routes != bmeta.groups.size()) {
				it->w /= 100;
			}

			sum += it->w;
		}

		struct {
			// reverse sort - from higher to lower weights
			bool operator()(const bw &b1, const bw &b2) {
				return b1.w > b2.w;
			}
		} cmp;
		std::sort(good_buckets.begin(), good_buckets.end(), cmp);

		// randomly select value in a range [0, sum+1)
		// then iterate over all good buckets starting from the one with the highest weight
		//
		// the higher the weight, the more likely this bucket will be selected
		float rnd = (0 + (rand() % (int)(sum * 10 - 0 + 1))) / 10.0;

		BH_LOG(log, DNET_LOG_NOTICE, "test: weight selection: good-buckets: %d, rnd: %f, sum: %f",
				good_buckets.size(), rnd, sum);

		for (auto it = good_buckets.rbegin(), end = good_buckets.rend(); it != end; ++it) {
			BH_LOG(log, DNET_LOG_NOTICE, "test: weight comparison: bucket: %s, sum: %f, rnd: %f, weight: %f",
					it->b->name().c_str(), sum, rnd, it->w);
			rnd -= it->w;
			if (rnd <= 0) {
				ret = it->b;
				break;
			}
		}

		return elliptics::error_info();
	}

	elliptics::error_info get_bucket(size_t size, std::string &bname) {
		bucket b;
		elliptics::error_info err = get_bucket(size, b);
		if (err)
			return err;

		bname = b->name();
		return elliptics::error_info();
	}

	elliptics::error_info find_bucket(const std::string &bname, bucket &b) {
		std::lock_guard<std::mutex> lock(m_lock);
		auto it = m_buckets.find(bname);
		if (it == m_buckets.end()) {
			return elliptics::create_error(-ENOENT, "could not find bucket '%s' in bucket list", bname.c_str());
		}

		if (!it->second->valid()) {
			return elliptics::create_error(-EINVAL, "bucket '%s' is not valid", bname.c_str());
		}

		b = it->second;
		return elliptics::error_info();
	}


	// run self test, raise exception if there are problems
	//
	// Tests:
	// 1. select bucket for upload multiple times,
	// 	check that distribution is biased towards buckets with more free space available
	void test() {
		elliptics::logger &log = m_node->get_log();
		BH_LOG(log, DNET_LOG_INFO, "test: start: buckets: %d", m_buckets.size());

		std::unique_lock<std::mutex> guard(m_lock);
		if (m_buckets.size() == 0) {
			throw std::runtime_error("there are no buckets at all");
		}

		limits l;

		struct bucket_weight {
			bucket		b;
			float		weight = 0;
			int		counter = 0;
		};

		std::vector<bucket_weight> good_buckets;
		std::vector<bucket_weight> really_good_buckets;

		float sum = 0;
		float really_good_sum = 0;
		for (auto it = m_buckets.begin(), end = m_buckets.end(); it != end; ++it) {
			if (it->second->valid()) {
				float w = it->second->weight(1, l);

				BH_LOG(log, DNET_LOG_NOTICE, "test: bucket: %s, weight: %f", it->second->name(), w);

				// skip buckets with zero weights
				// usually this means that there is no free space for this request
				// or stats are broken (timed out)
				if (w <= 0)
					continue;

				bucket_weight bw;
				bw.b = it->second;
				bw.weight = w;

				good_buckets.push_back(bw);
				sum += w;

				if (w > 0.5) {
					really_good_buckets.push_back(bw);
					really_good_sum += w;
				}
			}
		}

		guard.unlock();

		// use really good buckets if we have them
		if (really_good_sum > 0) {
			sum = really_good_sum;
			good_buckets.swap(really_good_buckets);
		}

		if (good_buckets.size() == 0) {
			throw std::runtime_error("there are buckets, but they are not suitable for size " +
					elliptics::lexical_cast(1));
		}


		// first test - call @get_bucket() many times, check that distribution
		// of the buckets looks similar to the initial weights
		int num = 10000;
		for (int i = 0; i < num; ++i) {
			std::string bname;
			elliptics::error_info err = get_bucket(1, bname);
			if (err) {
				throw std::runtime_error("get_bucket() failed: " + err.message());
			}

			auto bit = std::find_if(good_buckets.begin(), good_buckets.end(),
					[&](const bucket_weight &bw) { return bw.b->name() == bname; });
			if (bit != good_buckets.end()) {
				bit->counter++;
			}
		}

		float eq_min = 0.9;
		float eq_max = 1.1;
		for (auto it = good_buckets.begin(), end = good_buckets.end(); it != end; ++it) {
			float ratio = (float)it->counter/(float)num;
			float wratio = it->weight / sum;

			// @ratio is a number of this bucket selection related to total number of runs
			// it should rougly correspond to the ratio of free space in given bucket, or weight

			float eq = ratio / wratio;

			BH_LOG(log, DNET_LOG_INFO, "test: bucket: %s, weight: %f, counter: %d/%d, "
					"weight ratio: %f, selection ratio: %f, ratio/wratio: %.2f (must be in [%.2f, %.2f]",
					it->b->name(), it->weight,
					it->counter, num,
					wratio, ratio,
					eq, eq_min, eq_max);

			if (eq > eq_max || eq < eq_min) {
				std::ostringstream ss;
				ss << "bucket: " << it->b->name() <<
					", weight: " << it->weight <<
					", weight ratio: " << wratio <<
					", selection ratio: " << ratio <<
					": parameters mismatch, weight and selection ratios should be close to each other";
				throw std::runtime_error(ss.str());
			}
		}

		BH_LOG(log, DNET_LOG_INFO, "test: weight comparison of %d buckets has been completed", m_buckets.size());
	}


private:
	std::shared_ptr<elliptics::node> m_node;

	std::mutex m_lock;
	std::vector<int> m_meta_groups;

	std::string m_bucket_key;
	std::vector<std::string> m_bnames;
	std::map<std::string, bucket> m_buckets;

	elliptics_stat m_stat;

	elliptics::session m_error_session;

	bool m_need_exit = false;
	std::condition_variable m_wait;
	std::thread m_buckets_update;

	std::map<std::string, bucket> read_buckets(const std::vector<int> mgroups, const std::vector<std::string> &bnames) {
		std::map<std::string, bucket> buckets;

		for (auto it = bnames.begin(), end = bnames.end(); it != end; ++it) {
			buckets[*it] = make_bucket(m_node, mgroups, *it);
		}
		m_stat.schedule_update_and_wait();

		limits l;
		elliptics::logger &log = m_node->get_log();
		for (auto it = buckets.begin(), end = buckets.end(); it != end; ++it) {
			it->second->wait_for_reload();

			bucket_meta meta = it->second->meta();
			for (auto g = meta.groups.begin(), gend = meta.groups.end(); g != gend; ++g) {
				backend_stat bs = m_stat.stat(*g);
				if (bs.group == *g) {
					it->second->set_backend_stat(*g, bs);
				}
			}

			BH_LOG(log, DNET_LOG_INFO, "read_buckets: bucket: %s: reloaded, valid: %d, "
					"stats: %s, weight: %f",
					it->first.c_str(), it->second->valid(),
					it->second->stat_str().c_str(), it->second->weight(1, l));
		}

		return buckets;
	}

	void received_bucket_list(const elliptics::sync_read_result &result, const elliptics::error_info &error) {
		elliptics::logger &log = m_node->get_log();

		if (error) {
			BH_LOG(log, DNET_LOG_ERROR, "received_bucket_list: key: %s: could not read bucket list, error: %s [%d]",
					m_bucket_key.c_str(), error.message().c_str(), error.code());
			return;
		}

		const elliptics::data_pointer &file = result[0].file();

		std::vector<std::string> bnames;

		char *start = file.data<char>();
		for (size_t i = 0; i < file.size(); ++i) {
			char *n = file.data<char>() + i;
			if (*n == '\n' || i == file.size() - 1) {
				if (i == file.size() - 1) {
					n++;
				}

				std::string bname(start, n - start);
				bnames.emplace_back(bname);

				start = n + 1;
			}
		}

		std::lock_guard<std::mutex> guard(m_lock);
		m_bnames = bnames;
	}

	elliptics::error_info request_bucket_list(const std::string &key, bool sync) {
		elliptics::session s(*m_node);
		s.set_exceptions_policy(elliptics::session::no_exceptions);
		s.set_groups(m_meta_groups);

		std::string ns = "bucket";
		s.set_namespace(ns.c_str(), ns.size());

		auto ret = s.read_data(key, 0, 0);
		if (!ret.is_valid()) {
			return elliptics::create_error(-EINVAL, "async read data is not valid");
		}
		if (sync) {
			ret.wait();
			if (ret.error())
				return ret.error();

			received_bucket_list(ret.get(), ret.error());
		} else {
			ret.connect(std::bind(&bucket_processor::received_bucket_list, this,
					std::placeholders::_1, std::placeholders::_2));

		}

		return elliptics::error_info();
	}

	void buckets_update() {
		while (!m_need_exit) {
			if (!m_bucket_key.empty())
				request_bucket_list(m_bucket_key, true);

			std::unique_lock<std::mutex> guard(m_lock);
			if (m_wait.wait_for(guard, std::chrono::seconds(30), [&] {return m_need_exit;}))
				break;
			guard.unlock();

			std::map<std::string, bucket> buckets = read_buckets(m_meta_groups, m_bnames);

			guard.lock();
			m_buckets = buckets;
		}
	}
};

}} // namespace ioremap::ebucket

#endif // __EBUCKET_BUCKET_PROCESSOR_HPP

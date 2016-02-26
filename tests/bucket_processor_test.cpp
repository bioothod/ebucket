#include <algorithm>
#include <iostream>

#include "ebucket/bucket_processor.hpp"

#include <boost/program_options.hpp>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	std::vector<std::string> remotes;


	bpo::options_description generic("Ebucket example options");
	generic.add_options()
		("help", "this help message")
		;

	std::string log_file, log_level, groups_str;
	bpo::options_description ell("Elliptics options, this test will generate random bucket names, put them into bucket key and test, "
			"whether bucket key initialization works. If it works, common bucket processor test will be started.");
	ell.add_options()
		("remote", bpo::value<std::vector<std::string>>(&remotes)->required()->composing(), "remote node: addr:port:family")
		("log-file", bpo::value<std::string>(&log_file)->default_value("/dev/stdout"), "log file")
		("log-level", bpo::value<std::string>(&log_level)->default_value("error"), "log level: error, info, notice, debug")
		("groups", bpo::value<std::string>(&groups_str)->required(), "groups where bucket metadata is stored: 1:2:3")
		;

	bpo::options_description cmdline_options;
	cmdline_options.add(generic).add(ell);

	bpo::variables_map vm;

	try {
		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).run(), vm);

		if (vm.count("help")) {
			std::cout << cmdline_options << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << cmdline_options << std::endl;
		return -1;
	}

	std::vector<int> groups = elliptics::parse_groups(groups_str.c_str());

	elliptics::file_logger log(log_file.c_str(), elliptics::file_logger::parse_level(log_level));
	std::shared_ptr<elliptics::node> node(new elliptics::node(elliptics::logger(log, blackhole::log::attributes_t())));

	std::vector<elliptics::address> rem(remotes.begin(), remotes.end());
	node->add_remote(rem);

	ebucket::bucket_processor bp(node);

	std::string bucket_key = "bucket-key-" + std::to_string(time(NULL));
	std::ostringstream bucket_key_data;

	elliptics::session s(*node);
	std::string ns = "bucket";
	s.set_namespace(ns.data(), ns.size());
	s.set_groups(groups);
	s.set_exceptions_policy(elliptics::session::no_exceptions);

	srand(time(NULL));
	int num_buckets = rand() % 7 + 3;
	for (int i = 0; i < num_buckets; ++i) {
		ebucket::bucket_acl acl;
		acl.user = "writer";
		acl.token = "secure token";
		acl.flags = ebucket::bucket_acl::auth_flags::auth_write;

		ebucket::bucket_meta bmeta;
		bmeta.name = "bucket-test-" + std::to_string(i) + "." + std::to_string(rand());
		bmeta.groups = groups;
		bmeta.acl[acl.user] = acl;

		std::ostringstream ss;
		msgpack::pack(ss, bmeta);

		auto ret = s.write_data(bmeta.name, ss.str(), 0);
		if (!ret.is_valid()) {
			std::cerr << "could not write bucket meta, async result is not valid" << std::endl;
			return -EINVAL;
		}

		ret.wait();
		if (ret.error()) {
			std::cerr << "could not write bucket meta, error: " << ret.error().message() << std::endl;
			return ret.error().code();
		}

		std::cout << "successfully uploaded bucket " << bmeta.name << std::endl;
		bucket_key_data << bmeta.name;
		if (i != num_buckets - 1)
			bucket_key_data << "\n";
	}

	auto ret = s.write_data(bucket_key, bucket_key_data.str(), 0);
	if (!ret.is_valid()) {
		std::cerr << "could not write bucket key, async result is not valid" << std::endl;
		return -EINVAL;
	}

	ret.wait();
	if (ret.error()) {
		std::cerr << "could not write bucket key, error: " << ret.error().message() << std::endl;
		return ret.error().code();
	}
	std::cout << "successfully uploaded bucket key " << bucket_key << std::endl;

	if (!bp.init(groups, bucket_key)) {
		std::cerr << "Could not initialize bucket transport, exiting";
		return -1;
	}

	bp.test();

	return 0;
}

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

	std::vector<std::string> bnames;
	std::string log_file, log_level, groups;
	bpo::options_description ell("Elliptics options");
	ell.add_options()
		("remote", bpo::value<std::vector<std::string>>(&remotes)->required()->composing(), "remote node: addr:port:family")
		("log-file", bpo::value<std::string>(&log_file)->default_value("/dev/stdout"), "log file")
		("log-level", bpo::value<std::string>(&log_level)->default_value("error"), "log level: error, info, notice, debug")
		("groups", bpo::value<std::string>(&groups)->required(), "groups where bucket metadata is stored: 1:2:3")
		("bucket", bpo::value<std::vector<std::string>>(&bnames)->composing(), "use these buckets in example")
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

	elliptics::file_logger log(log_file.c_str(), elliptics::file_logger::parse_level(log_level));
	std::shared_ptr<elliptics::node> node(new elliptics::node(elliptics::logger(log, blackhole::log::attributes_t())));

	std::vector<elliptics::address> rem(remotes.begin(), remotes.end());
	node->add_remote(rem);

	ebucket::bucket_processor bp(node);

	if (!bp.init(elliptics::parse_groups(groups.c_str()), bnames)) {
		std::cerr << "Could not initialize bucket transport, exiting";
		return -1;
	}

	for (int i = 0; i < 10; ++i) {
		std::string key = "this is a key " + std::to_string(i);
		std::string data = "this is some data " + std::to_string(i);
		ebucket::bucket b;

		elliptics::error_info err = bp.get_bucket(data.size(), b);
		if (err) {
			std::cerr << "Could not find bucket for size " << data.size() << " : " << err.message() << std::endl;
			return err.code();
		}

		elliptics::session s = b->session();

		auto ret = s.write_data(key, data, 0);
		ret.wait();
		if (!ret.is_valid() || ret.error()) {
			std::cerr << "Could not write data into bucket " << b->name() <<
				", size: " << data.size() <<
				", valid: " << ret.is_valid() <<
				", error: " << ret.error().message() <<
				std::endl;
			return err.code();
		}

		s.set_filter(elliptics::filters::positive);
		auto read_ret = s.read_data(key, 0, 0);
		if (!read_ret.is_valid() || read_ret.error()) {
			std::cerr << "Could not read data from bucket " << b->name() <<
				", valid: " << read_ret.is_valid() <<
				", error: " << read_ret.error().message() <<
				std::endl;
			return err.code();
		}

		if (read_ret.get_one().file().to_string() != data) {
			std::cerr << "Read invalid data" << std::endl;
			return -EINVAL;
		}

		std::cout << "Completed read/write from bucket " << b->name() << std::endl;
	}

	return 0;
}

#include "test.hh"

#include <configuration.hh>
#include "../../src/filter.cc"

using namespace kcov;

TEST(filter)
{
	std::string filename = std::string(crpcut::get_start_dir()) + "/test-binary";

	IConfiguration &conf = IConfiguration::getInstance();
	Filter &filter = (Filter &)IFilter::getInstance();
	bool res;
	const char *argv[] = {NULL, "/tmp/vobb", filename.c_str(), "tjena"};
	res = conf.parse(4, argv);
	ASSERT_TRUE(res);
	filter.setup();

	res = filter.runFilters("");
	ASSERT_TRUE(res);
	res = filter.runFilters(filename);
	ASSERT_TRUE(res);


	const char *argv2[] = {NULL, "--include-pattern=test-bin", "/tmp/vobb", filename.c_str(), "tjena"};
	res = conf.parse(5, argv2);
	ASSERT_TRUE(res);
	filter.setup();

	res = filter.runFilters(filename);
	ASSERT_TRUE(res);
	res = filter.runFilters("ingenting");
	ASSERT_FALSE(res);


	const char *argv3[] = {NULL, "--exclude-pattern=hej,hopp", "--include-pattern=bin", "/tmp/vobb", filename.c_str(), "tjena"};
	res = conf.parse(5, argv3);
	ASSERT_TRUE(res);
	filter.setup();

	res = filter.runFilters("binary");
	ASSERT_TRUE(res);
	res = filter.runFilters("hopp/binary");
	ASSERT_FALSE(res);
	res = filter.runFilters("hej/binary");
	ASSERT_FALSE(res);
	res = filter.runFilters("varken-eller");
	ASSERT_FALSE(res);

	const char *argv4[] = {NULL,
			(std::string("--include-path=") + crpcut::get_start_dir()).c_str(),
			"/tmp/vobb",
			filename.c_str(), "tjena"};
	res = conf.parse(5, argv4);
	ASSERT_TRUE(res);
	filter.setup();

	res = filter.runFilters(crpcut::get_start_dir());
	ASSERT_TRUE(res);
	res = filter.runFilters(filename);
	ASSERT_TRUE(res);
	res = filter.runFilters("hejsan-hoppsan");
	ASSERT_FALSE(res);

	const char *argv5[] = {NULL,
			(std::string("--exclude-path=") + crpcut::get_start_dir()).c_str(),
			"/tmp/vobb",
			filename.c_str(), "tjena"};
	res = conf.parse(5, argv5);
	ASSERT_TRUE(res);
	filter.setup();

	res = filter.runFilters(crpcut::get_start_dir());
	ASSERT_FALSE(res);
	res = filter.runFilters("/tmp");
	ASSERT_TRUE(res);
}

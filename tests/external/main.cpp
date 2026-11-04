#include <GstLog.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    const auto root = std::filesystem::path(argv[1]);
    auto logger = GST::LOG::get_Instance();
    GST::LOG::LogConfig config;
    config._logger_name = "service";
    config._log_type = "async";
    config._log_target = (root / "logs").string();
    config._log_file_name = "service.log";
    config._log_level = GST::LOG::LOG_LEVEL::LEVEL_INFO;
    config._log_format = "%S";
    config._natural_rotation = GST::LOG::NaturalRotation::Hourly;
    if (!logger->init(config)) {
        return 2;
    }
    INFO_NAME("service", "external=%d", 17);
    if (!logger->try_log("service", GST::LOG::LOG_LEVEL::LEVEL_INFO, __FILE__, __LINE__, __func__, "%c%c%c", 'A', 0, 'B')) {
        return 3;
    }
    if (!logger->flush() || !logger->shutdown()) {
        return 4;
    }
    std::ifstream file(root / "logs/service.log", std::ios::binary);
    const std::string actual{std::istreambuf_iterator<char>(file), {}};
    const auto expected = std::string("external=17\n") + std::string("A\0B\n", 4);
    return actual == expected ? 0 : 5;
}

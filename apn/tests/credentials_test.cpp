#include <apn/types/credentials.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <userver/utest/utest.hpp>

namespace {

constexpr std::string_view kTestPem = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg0SDXkyuVprFEPSE7
8N8EiGiRBZ7D0zPakBVeklUlsDyhRANCAARI0sBcbBmalQeSxlasnuBXgeHD1EOC
UYVLhKa2JEIaaT7WMrsBptha+2THo4mx4YEpwu3KXq2bgbelo8jDsH7W
-----END PRIVATE KEY-----
)";

class TempPemFile {
public:
    TempPemFile(std::string_view contents) {
        path_ = std::filesystem::temp_directory_path() /
                ("apn_test_" + std::to_string(::getpid()) + "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".pem");
        std::ofstream out{path_};
        out << contents;
    }
    ~TempPemFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    [[nodiscard]] auto Path() const -> std::string { return path_.string(); }

private:
    std::filesystem::path path_;
};

}  // namespace

TEST(ApnCredentials, ResolvePemReturnsInlineWhenSet) {
    apn::Credentials creds;
    creds.key_pem = std::string{kTestPem};
    creds.key_pem_file = "/nonexistent/should-not-be-read.pem";

    EXPECT_EQ(creds.ResolvePem(), std::string{kTestPem});
}

TEST(ApnCredentials, ResolvePemReadsFileWhenInlineEmpty) {
    TempPemFile file{kTestPem};
    apn::Credentials creds;
    creds.key_pem_file = file.Path();

    EXPECT_EQ(creds.ResolvePem(), std::string{kTestPem});
}

TEST(ApnCredentials, ResolvePemThrowsWhenNeitherSet) {
    apn::Credentials creds;
    EXPECT_THROW(creds.ResolvePem(), std::runtime_error);
}

TEST(ApnCredentials, ResolvePemThrowsWhenFileMissing) {
    apn::Credentials creds;
    creds.key_pem_file = "/nonexistent/path/to/key.pem";
    EXPECT_THROW(creds.ResolvePem(), std::runtime_error);
}

TEST(ApnCredentials, ResolvePemThrowsWhenFileEmpty) {
    TempPemFile file{""};
    apn::Credentials creds;
    creds.key_pem_file = file.Path();
    EXPECT_THROW(creds.ResolvePem(), std::runtime_error);
}

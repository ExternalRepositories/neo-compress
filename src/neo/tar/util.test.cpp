#include <neo/tar/util.hpp>

#include <neo/gzip.hpp>
#include <neo/inflate.hpp>
#include <neo/tar/ustar.hpp>

#include <neo/as_dynamic_buffer.hpp>
#include <neo/iostream_io.hpp>
#include <neo/string_io.hpp>
#include <neo/transform_io.hpp>

#include <catch2/catch.hpp>

#include <fstream>

namespace fs = std::filesystem;

const auto THIS_DIR  = fs::path(__FILE__).parent_path();
const auto ROOT      = THIS_DIR.parent_path().parent_path().parent_path();
const auto BUILD_DIR = ROOT / "_build";

TEST_CASE("Compress a directory") {
    auto dest = BUILD_DIR / "test-compress.tar.gz";
    neo::compress_directory_targz(THIS_DIR.parent_path(), dest);

    neo::buffer_transform_source gz_data{
        neo::iostream_io{std::ifstream{dest, std::ios::binary}},
        neo::gzip_decompressor{neo::inflate_decompressor{}},
    };

    neo::ustar_reader reader{gz_data};
    auto              mem = reader.next_member().value();
    CHECK(mem.filename_str() != "");
}

TEST_CASE("Expand a directory") {
    auto dest = BUILD_DIR / "test-expand.dir";
    fs::remove_all(dest);
    fs::create_directories(dest);
    neo::expand_directory_targz(dest, ROOT / "data/test.tar.gz");
    CHECK(fs::is_regular_file(dest / "01-test.txt"));
    CHECK(fs::is_regular_file(dest / "02-test.txt"));
    CHECK(fs::is_directory(dest / "subdir"));
    CHECK(fs::is_regular_file(dest / "subdir/thing.txt"));
    CHECK(std::distance(fs::recursive_directory_iterator(dest), fs::recursive_directory_iterator())
          == 4);

    neo::string_dynbuf_io str;
    neo::buffer_copy(str,
                     neo::iostream_io(std::ifstream{dest / "subdir/thing.txt", std::ios::binary}));
    CHECK(str.read_area_view()
          == "I'm just another file, but in a subdirectory!\n\n- The Sign Painter");
}

TEST_CASE("Expand an archive containing pax extensions") {
    auto dest = BUILD_DIR / "test-expand.dir";
    fs::remove_all(dest);
    fs::create_directories(dest);
    auto tgz_in = ROOT / "data/neo-buffer-0.4.2.tar.gz";
    neo::expand_directory_targz(dest, tgz_in);
    CHECK(fs::is_regular_file(dest / "neo-buffer-0.4.2/package.jsonc"));

    neo::string_dynbuf_io str;
    neo::buffer_copy(str,
                     neo::iostream_io(
                         std::ifstream{dest / "neo-buffer-0.4.2/package.jsonc", std::ios::binary}));
    CHECK(str.read_area_view() == R"({
    "name": "neo-buffer",
    "version": "0.4.2",
    "namespace": "neo",
    "test_driver": "Catch-Main",
    "depends": [
        "neo-concepts^0.4.0",
        "neo-fun^0.4.1"
    ]
})");

    fs::remove_all(dest);
    fs::create_directories(dest);
    neo::expand_directory_targz(
        neo::expand_options{
            .destination_directory = dest,
            .input_name            = tgz_in.string(),
            .strip_components      = 1,
        },
        tgz_in);
    // We've stripped on directory component
    CHECK(fs::is_regular_file(dest / "package.jsonc"));
}

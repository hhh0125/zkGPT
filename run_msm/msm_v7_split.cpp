#include <mcl/bn.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using mcl::bn::G1;
using mcl::bn::Fr;

namespace {
// 读取.npy文件
struct NpyHeader {
    std::string descr;
    bool fortran_order = false;
    std::vector<size_t> shape;
    std::streamoff data_offset = 0;
};


enum class ScalarFormatKind {
    Bn254,   //  没有正负号
    Bn254Balanced,     // 有正负号
    Signed128,        // 128位有符号整数
};

struct Options {
    std::string scalars_path;
    std::string points_path;
    std::string points_mode = "random";
    std::string point_seed;
    std::string out_path;
    std::string stats_path;

    size_t K = 0;
    size_t n = 0;
    size_t row_start = 0;
    size_t row_count = 0;
    size_t point_offset = 0;

    int c = 0;
    int scalar_bits = 254;
    std::string scalar_format = "bn254-balanced";
    ScalarFormatKind scalar_format_kind = ScalarFormatKind::Bn254Balanced;

    unsigned threads = 0;
    size_t progress_every = 64;
    std::string mode = "tile-mask-fast64";
    size_t tile_size = 64;

    // fast64 exact-mask sharing thresholds.
    uint64_t fast_min_gain = 32;
    uint32_t fast_min_group_count = 2;

    // Existing command compatibility: --split-min-gain is used as the
    // default value of --fast-min-gain when the latter is omitted.
    int split_min_gain = 32;
    size_t split_base_tile_size = 32;
    size_t split_factor = 2;

    // Empty means all windows use fast64. Selected-out windows fall back to
    // direct per-row bucket accumulation inside tile-mask-fast64 mode.
    std::string fast_windows_spec;
    std::vector<int> fast_windows;
    bool fast_windows_enabled = false;

    // Aligned 64->child exact-mask multiscale selection.
    bool fast_multiscale = false;
    std::string fast_multiscale_decision = "compare_nosplit_refine";
};

// 模数
const uint64_t BN254_FR_MODULUS[4] = {
    0xa10000000000000dULL,
    0xff9f800000000010ULL,
    0xba344d8000000007ULL,
    0x2523648240000001ULL,
};

const uint64_t BN254_FR_HALF[4] = {
    0x5080000000000006ULL,
    0xffcfc00000000008ULL,
    0xdd1a26c000000003ULL,
    0x1291b24120000000ULL,
};


// Per-worker wall-time attribution.
struct ThreadWallTimeRecord {
    unsigned thread_id = 0;
    double count_build_seconds = 0.0;
    double g1_group_build_seconds = 0.0;
    double running_sum_seconds = 0.0;
    double total_worker_seconds = 0.0;
    double tile_scalar_alloc_seconds = 0.0;
    double tile_scalar_read_seconds = 0.0;
    double fast64_workspace_init_seconds = 0.0;
    double tile_output_serialize_seconds = 0.0;
    double stats_atomic_merge_seconds = 0.0;
    double tile_dispatch_wait_seconds = 0.0;
    double barrier_wait_seconds = 0.0;
};

// Per-tile statistics returned by tile_direct_pippenger_rows() or
// pippenger_tile_mask_fast64_rows().
struct TileMaskStats {
    double event_precompute_seconds = 0.0;
    double count_build_seconds = 0.0;
    double count_scan_seconds = 0.0;
    double count_finalize_seconds = 0.0;
    double direct_window_seconds = 0.0;
    double g1_group_build_seconds = 0.0;
    double group_event_loop_seconds = 0.0;
    double singleton_direct_seconds = 0.0;
    double running_sum_seconds = 0.0;
    double scalar_predecode_seconds = 0.0;
    double direct_bucket_accum_seconds = 0.0;
    double tile_workspace_clear_seconds = 0.0;
    double fast64_workspace_init_seconds = 0.0;

    uint64_t zero_tile_skipped_count = 0;
    uint64_t bucket_point_adds = 0;
    uint64_t raw_row_bucket_adds = 0;
    uint64_t optimized_bucket_adds_est = 0;
    uint64_t exact_mask_saving_est = 0;
    uint64_t shared_group_point_count = 0;
    uint64_t fast_direct_row_bucket_adds = 0;
    uint64_t shared_bucket_groups = 0;
    uint64_t shared_bucket_row_links = 0;
    uint64_t group_point_visits = 0;
    uint64_t share_point_events = 0;
    uint64_t fallback_point_events = 0;
    uint64_t fallback_row_adds = 0;
    uint64_t fallback_popcount1_events = 0;
    uint64_t shared_link_row_adds = 0;
    uint64_t single_leaf_fast_slots = 0;
    uint64_t multi_leaf_fast_slots = 0;
    uint64_t shareable_mask_entries = 0;
    uint64_t shareable_mask_hits = 0;
    uint64_t nonshareable_mask_misses = 0;
    uint64_t singleton_direct_point_events = 0;
    uint64_t singleton_direct_row_adds = 0;
    uint64_t multiscale_parent_chosen = 0;
    uint64_t multiscale_child_chosen = 0;

    uint64_t scalar_predecode_tiles = 0;
    uint64_t scalar_predecode_nonzero_scalars = 0;
    uint64_t scalar_predecode_zero_scalars = 0;
    uint64_t scalar_predecode_capacity = 0;

    uint64_t memory_fast64_row_buckets_max_bytes = 0;
    uint64_t memory_fast64_bucket_point_masks_max_bytes = 0;
    uint64_t memory_fast64_nz_scalars_max_bytes = 0;
    uint64_t memory_fast64_first_hits_max_bytes = 0;
    uint64_t memory_fast64_static_workspace_max_bytes = 0;
    uint64_t memory_fast64_estimated_peak_tile_max_bytes = 0;
    uint64_t memory_fast64_window_events_max_bytes = 0;
};

// Whole-run statistics.
struct TimingStats {
    double init_seconds = 0.0;
    double load_points_seconds = 0.0;
    double scalar_header_seconds = 0.0;
    double compute_seconds = 0.0;
    double tile_direct_sparse_seconds = 0.0;
    double direct_bucket_accum_seconds = 0.0;
    double tile_count_build_seconds = 0.0;
    double tile_count_scan_seconds = 0.0;
    double tile_count_finalize_seconds = 0.0;
    double tile_direct_window_seconds = 0.0;
    double tile_g1_group_build_seconds = 0.0;
    double tile_group_event_loop_seconds = 0.0;
    double tile_singleton_direct_seconds = 0.0;
    double tile_running_sum_seconds = 0.0;
    double scalar_predecode_seconds = 0.0;
    double event_precompute_seconds = 0.0;
    double tile_workspace_clear_seconds = 0.0;
    double tile_dispatch_wait_seconds = 0.0;
    double barrier_wait_seconds = 0.0;
    double tile_scalar_alloc_seconds = 0.0;
    double tile_scalar_read_seconds = 0.0;
    double fast64_workspace_init_seconds = 0.0;
    double tile_output_serialize_seconds = 0.0;
    double stats_atomic_merge_seconds = 0.0;
    double write_output_seconds = 0.0;
    double total_seconds = 0.0;

    uint64_t bucket_point_adds = 0;
    uint64_t raw_row_bucket_adds = 0;
    uint64_t optimized_bucket_adds_est = 0;
    uint64_t exact_mask_saving_est = 0;
    uint64_t shared_group_point_count = 0;
    uint64_t fast_direct_row_bucket_adds = 0;
    uint64_t shared_bucket_groups = 0;
    uint64_t shared_bucket_row_links = 0;
    uint64_t group_point_visits = 0;
    uint64_t share_point_events = 0;
    uint64_t fallback_point_events = 0;
    uint64_t fallback_row_adds = 0;
    uint64_t fallback_popcount1_events = 0;
    uint64_t shared_link_row_adds = 0;
    uint64_t single_leaf_fast_slots = 0;
    uint64_t multi_leaf_fast_slots = 0;
    uint64_t shareable_mask_entries = 0;
    uint64_t shareable_mask_hits = 0;
    uint64_t nonshareable_mask_misses = 0;
    uint64_t singleton_direct_point_events = 0;
    uint64_t singleton_direct_row_adds = 0;
    uint64_t multiscale_parent_chosen = 0;
    uint64_t multiscale_child_chosen = 0;
    uint64_t zero_tile_skipped_count = 0;

    uint64_t scalar_predecode_tiles = 0;
    uint64_t scalar_predecode_nonzero_scalars = 0;
    uint64_t scalar_predecode_zero_scalars = 0;
    uint64_t scalar_predecode_capacity = 0;

    uint64_t memory_fast64_row_buckets_max_bytes = 0;
    uint64_t memory_fast64_bucket_point_masks_max_bytes = 0;
    uint64_t memory_fast64_nz_scalars_max_bytes = 0;
    uint64_t memory_fast64_first_hits_max_bytes = 0;
    uint64_t memory_fast64_static_workspace_max_bytes = 0;
    uint64_t memory_fast64_estimated_peak_tile_max_bytes = 0;
    uint64_t memory_fast64_window_events_max_bytes = 0;

    std::vector<ThreadWallTimeRecord> thread_wall_times;
};

double elapsed_seconds(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double>(end - begin).count();
}

uint64_t seconds_to_us(double seconds) {
    if (seconds <= 0.0) return 0;
    return static_cast<uint64_t>(seconds * 1000000.0 + 0.5);
}

void atomic_update_max(std::atomic<uint64_t>& target, uint64_t value) {
    uint64_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
           !target.compare_exchange_weak(
               current,
               value,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

uint16_t load_u16_le(const unsigned char *p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t load_u32_le(const unsigned char *p) {
    return static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string bytes_to_hex(const unsigned char *buf, size_t n) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(buf[i]);
    }
    return oss.str();
}

std::string limbs_to_string(const std::vector<uint64_t>& row) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < row.size(); ++i) {
        if (i) oss << ", ";
        oss << row[i];
    }
    oss << "]";
    return oss.str();
}
// 从 NPY Python 字典形式的 header 中读取字符串字段
std::string extract_quoted_value(const std::string& header, const std::string& key) {
    const std::string marker = "'" + key + "'";
    size_t pos = header.find(marker);
    if (pos == std::string::npos) {
        pos = header.find("\"" + key + "\"");
    }
    if (pos == std::string::npos) {
        throw std::runtime_error("NPY header missing key: " + key);
    }
    pos = header.find(':', pos);
    if (pos == std::string::npos) {
        throw std::runtime_error("Malformed NPY header near key: " + key);
    }
    size_t quote = header.find_first_of("'\"", pos + 1);
    if (quote == std::string::npos) {
        throw std::runtime_error("Malformed quoted NPY value for key: " + key);
    }
    char q = header[quote];
    size_t end = header.find(q, quote + 1);
    if (end == std::string::npos) {
        throw std::runtime_error("Unterminated quoted NPY value for key: " + key);
    }
    return header.substr(quote + 1, end - quote - 1);
}

bool extract_bool_value(const std::string& header, const std::string& key) {
    const std::string marker = "'" + key + "'";
    size_t pos = header.find(marker);
    if (pos == std::string::npos) {
        pos = header.find("\"" + key + "\"");
    }
    if (pos == std::string::npos) {
        throw std::runtime_error("NPY header missing key: " + key);
    }
    pos = header.find(':', pos);
    if (pos == std::string::npos) {
        throw std::runtime_error("Malformed NPY header near key: " + key);
    }
    size_t end = header.find(',', pos + 1);
    std::string value = trim(header.substr(pos + 1, end - pos - 1));
    if (value == "False") return false;
    if (value == "True") return true;
    throw std::runtime_error("Unsupported NPY bool value for key " + key + ": " + value);
}

std::vector<size_t> extract_shape(const std::string& header) {
    size_t key = header.find("'shape'");
    if (key == std::string::npos) {
        key = header.find("\"shape\"");
    }
    if (key == std::string::npos) {
        throw std::runtime_error("NPY header missing shape");
    }
    size_t l = header.find('(', key);
    size_t r = header.find(')', l);
    if (l == std::string::npos || r == std::string::npos || r <= l) {
        throw std::runtime_error("Malformed NPY shape");
    }
    std::string inside = header.substr(l + 1, r - l - 1);
    std::vector<size_t> shape;
    std::stringstream ss(inside);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) continue;
        shape.push_back(static_cast<size_t>(std::stoull(token)));
    }
    return shape;
}

/**************读取.npy文件头*************/
NpyHeader read_npy_header(std::ifstream& in, const std::string& path) {
    char magic[6];
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, "\x93NUMPY", 6) != 0) {
        throw std::runtime_error(path + " is not a .npy file");
    }

    unsigned char version[2];
    in.read(reinterpret_cast<char*>(version), sizeof(version));
    if (!in) {
        throw std::runtime_error("Failed to read NPY version from " + path);
    }

    uint32_t header_len = 0;
    if (version[0] == 1) {
        unsigned char len_bytes[2];
        in.read(reinterpret_cast<char*>(len_bytes), sizeof(len_bytes));
        header_len = load_u16_le(len_bytes);
    } else if (version[0] == 2 || version[0] == 3) {
        unsigned char len_bytes[4];
        in.read(reinterpret_cast<char*>(len_bytes), sizeof(len_bytes));
        header_len = load_u32_le(len_bytes);
    } else {
        throw std::runtime_error("Unsupported NPY version in " + path);
    }

    std::string header(header_len, '\0');
    in.read(&header[0], header.size());
    if (!in) {
        throw std::runtime_error("Failed to read NPY header from " + path);
    }

    NpyHeader out;
    out.descr = extract_quoted_value(header, "descr");
    out.fortran_order = extract_bool_value(header, "fortran_order");
    out.shape = extract_shape(header);
    out.data_offset = in.tellg();
    if (out.descr != "<u8" && out.descr != "|u8") {
        throw std::runtime_error("Only uint64 NPY arrays are supported, got descr=" + out.descr);
    }
    if (out.fortran_order) {
        throw std::runtime_error("Fortran-order NPY arrays are not supported");
    }
    return out;
}
// 作用于输出文件
std::string make_npy_header(size_t rows, size_t cols) {
    std::ostringstream oss;
    oss << "{'descr': '<u8', 'fortran_order': False, 'shape': ("
        << rows << ", " << cols << "), }";
    std::string header = oss.str();
    header.push_back('\n');
    while ((10 + header.size()) % 16 != 0) {
        header.insert(header.end() - 1, ' ');
    }
    return header;
}

void write_npy_u64_2d(const std::string& path, const std::vector<uint64_t>& data, size_t rows, size_t cols) {
    if (data.size() != rows * cols) {
        throw std::runtime_error("Internal output size mismatch");
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }
    const char magic[] = "\x93NUMPY";
    out.write(magic, 6);
    const unsigned char version[2] = {1, 0};
    out.write(reinterpret_cast<const char*>(version), 2);
    std::string header = make_npy_header(rows, cols);
    if (header.size() > 0xffff) {
        throw std::runtime_error("NPY v1 header too large");
    }
    const uint16_t hlen = static_cast<uint16_t>(header.size());
    unsigned char len_bytes[2] = {
        static_cast<unsigned char>(hlen & 0xff),
        static_cast<unsigned char>((hlen >> 8) & 0xff),
    };
    out.write(reinterpret_cast<const char*>(len_bytes), 2);
    out.write(header.data(), header.size());
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(uint64_t)));
    if (!out) {
        throw std::runtime_error("Failed while writing output file: " + path);
    }
}

uint64_t read_u64_arg(const std::map<std::string, std::string>& args, const std::string& name, uint64_t def = 0) {
    auto it = args.find(name);
    if (it == args.end()) return def;
    return static_cast<uint64_t>(std::stoull(it->second));
}

int read_i_arg(const std::map<std::string, std::string>& args, const std::string& name, int def = 0) {
    auto it = args.find(name);
    if (it == args.end()) return def;
    return std::stoi(it->second);
}

std::string read_s_arg(const std::map<std::string, std::string>& args, const std::string& name, const std::string& def = "") {
    auto it = args.find(name);
    if (it == args.end()) return def;
    return it->second;
}

std::vector<std::string> split_csv(const std::string& text) {
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (!token.empty()) out.push_back(token);
    }
    return out;
}

std::vector<int> parse_int_csv(const std::string& text, const std::string& arg_name) {
    std::vector<int> values;
    if (trim(text).empty()) return values;
    for (const std::string& token : split_csv(text)) {
        try {
            values.push_back(std::stoi(token));
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid integer in " + arg_name + ": " + token);
        }
    }
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

bool contains_int(const std::vector<int>& values, int x) {
    return std::binary_search(values.begin(), values.end(), x);
}

void serialize_g1_row(const G1& value, std::vector<uint64_t>& output, size_t local_row) {
    unsigned char buf[32];
    const size_t written = value.serialize(buf, sizeof(buf), mcl::IoSerialize);
    if (written != sizeof(buf)) {
        std::ostringstream oss;
        oss << "G1 serialize failed at output row " << local_row
            << ", bytes_written=" << written;
        throw std::runtime_error(oss.str());
    }
    std::memcpy(&output[local_row * 4], buf, sizeof(buf));
}

bool fast64_enabled_for_window(const Options& opt, int window) {
    if (!opt.fast_windows_enabled) return true;
    return contains_int(opt.fast_windows, window);
}


Options parse_args(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) != 0) {
            throw std::runtime_error("Unexpected positional argument: " + key);
        }
        if (i + 1 >= argc) {
            throw std::runtime_error("Missing value for argument: " + key);
        }
        args[key] = argv[++i];
    }

    Options opt;
    opt.scalars_path = read_s_arg(args, "--scalars");
    opt.points_path = read_s_arg(args, "--points");
    opt.points_mode = read_s_arg(args, "--points-mode", "random");
    opt.point_seed = read_s_arg(args, "--point-seed", "");
    opt.out_path = read_s_arg(args, "--out");
    opt.stats_path = read_s_arg(args, "--stats");
    opt.K = static_cast<size_t>(read_u64_arg(args, "--K"));
    opt.n = static_cast<size_t>(read_u64_arg(args, "--n"));
    opt.row_start = static_cast<size_t>(read_u64_arg(args, "--row-start", 0));
    opt.row_count = static_cast<size_t>(read_u64_arg(args, "--row-count", 0));
    opt.point_offset = static_cast<size_t>(read_u64_arg(args, "--point-offset", 0));
    opt.c = read_i_arg(args, "--c");
    opt.scalar_bits = read_i_arg(args, "--scalar-bits", 254);
    opt.scalar_format = read_s_arg(args, "--scalar-format", "bn254-balanced");
    opt.threads = static_cast<unsigned>(read_u64_arg(args, "--threads", 0));
    opt.progress_every = static_cast<size_t>(read_u64_arg(args, "--progress-every", 64));
    opt.mode = read_s_arg(args, "--mode", "tile-mask-fast64");
    opt.tile_size = static_cast<size_t>(read_u64_arg(args, "--tile-size", 64));
    opt.split_min_gain = read_i_arg(args, "--split-min-gain", 32);
    opt.split_base_tile_size = static_cast<size_t>(read_u64_arg(args, "--split-base-tile-size", 32));
    opt.split_factor = static_cast<size_t>(read_u64_arg(args, "--split-factor", 2));
    opt.fast_min_gain = read_u64_arg(args, "--fast-min-gain", static_cast<uint64_t>(opt.split_min_gain));
    opt.fast_min_group_count = static_cast<uint32_t>(read_u64_arg(args, "--fast-min-group-count", 2));
    opt.fast_windows_spec = read_s_arg(args, "--fast-windows", "");
    opt.fast_multiscale = read_i_arg(args, "--fast-multiscale", 0) != 0;
    opt.fast_multiscale_decision = read_s_arg(args, "--fast-multiscale-decision", "compare_nosplit_refine");
    opt.fast_windows = parse_int_csv(opt.fast_windows_spec, "--fast-windows");
    opt.fast_windows_enabled = !opt.fast_windows.empty();


    if (opt.scalars_path.empty()) {
        throw std::runtime_error("Required argument: --scalars");
    }
    if (opt.out_path.empty()) {
        throw std::runtime_error("Required argument: --out");
    }
    if (opt.points_mode != "random" && opt.points_mode != "file") {
        throw std::runtime_error("--points-mode must be random or file");
    }
    if (opt.points_mode == "file" && opt.points_path.empty()) {
        throw std::runtime_error("--points is required when --points-mode file");
    }
    if (opt.K == 0 || opt.n == 0) {
        throw std::runtime_error("--K and --n must be positive");
    }
    if (opt.row_count == 0) {
        opt.row_count = opt.K - opt.row_start;
    }
    if (opt.row_start > opt.K || opt.row_count > opt.K - opt.row_start) {
        throw std::runtime_error("--row-start/--row-count exceed K");
    }
    if (opt.c <= 0 || opt.c > 20) {
        throw std::runtime_error("--c must be in [1, 20]");
    }
    if (opt.scalar_format == "bn254") {
        opt.scalar_format_kind = ScalarFormatKind::Bn254;
    } else if (opt.scalar_format == "bn254-balanced") {
        opt.scalar_format_kind = ScalarFormatKind::Bn254Balanced;
    } else if (opt.scalar_format == "signed128") {
        opt.scalar_format_kind = ScalarFormatKind::Signed128;
    } else {
        throw std::runtime_error("--scalar-format must be bn254, bn254-balanced, or signed128");
    }
    const int max_scalar_bits = (opt.scalar_format_kind == ScalarFormatKind::Signed128) ? 128 : 256;
    if (opt.scalar_bits <= 0 || opt.scalar_bits > max_scalar_bits) {
        std::ostringstream oss;
        oss << "--scalar-bits must be in [1, " << max_scalar_bits << "] for scalar-format="
            << opt.scalar_format;
        throw std::runtime_error(oss.str());
    }
    const int fast_window_count = (opt.scalar_bits + opt.c - 1) / opt.c;
    for (int w : opt.fast_windows) {
        if (w < 0 || w >= fast_window_count) {
            std::ostringstream oss;
            oss << "--fast-windows entry " << w << " is outside [0," << (fast_window_count - 1) << "]";
            throw std::runtime_error(oss.str());
        }
    }
    if (opt.mode != "tile-mask-fast64" && opt.mode != "pippenger") {
        throw std::runtime_error("Supported --mode values: tile-mask-fast64, pippenger");
    }
    if (opt.tile_size == 0 || opt.tile_size > opt.row_count) {
        throw std::runtime_error("--tile-size must be in [1, row_count]");
    }
    if (opt.tile_size > 64) {
        throw std::runtime_error("--mode tile-mask-fast64/pippenger requires --tile-size <= 64");
    }
    if (opt.fast_min_group_count < 2) {
        throw std::runtime_error("--fast-min-group-count must be at least 2");
    }
    if (opt.split_min_gain < 0) {
        throw std::runtime_error("--split-min-gain must be nonnegative");
    }
    if (opt.split_base_tile_size == 0) {
        throw std::runtime_error("--split-base-tile-size must be positive");
    }
    if (opt.split_factor < 2) {
        throw std::runtime_error("--split-factor must be at least 2");
    }
    if (opt.fast_multiscale_decision != "compare_nosplit_refine") {
        throw std::runtime_error("--fast-multiscale-decision currently supports only compare_nosplit_refine");
    }
    if (opt.fast_multiscale && opt.split_base_tile_size > opt.tile_size) {
        throw std::runtime_error("--split-base-tile-size must be <= --tile-size when --fast-multiscale 1");
    }
    if (opt.threads == 0) {
        opt.threads = std::thread::hardware_concurrency();
        if (opt.threads == 0) opt.threads = 1;
    }
    return opt;
}

void validate_scalar_shape(const NpyHeader& h, size_t K, size_t n) {
    if (h.shape.size() == 3 && h.shape[0] == K && h.shape[1] == n && h.shape[2] == 4) {
        return;
    }
    if (h.shape.size() == 2 && h.shape[0] == K * n && h.shape[1] == 4) {
        return;
    }
    if (h.shape.size() == 2 && h.shape[0] == K && h.shape[1] == n * 4) {
        return;
    }
    std::ostringstream oss;
    oss << "Unsupported scalar shape; expected (" << (K * n) << ",4), ("
        << K << "," << n << ",4), or (" << K << "," << (n * 4) << ")";
    throw std::runtime_error(oss.str());
}

std::streamoff scalar_row_offset(const NpyHeader& h, size_t row, size_t n) {
    const size_t u64_index = row * n * 4;
    return h.data_offset + static_cast<std::streamoff>(u64_index * sizeof(uint64_t));
}

Fr scalar_for_point_index(size_t i, const std::string& seed) {
    Fr s;
    if (seed.empty()) {
        s.setByCSPRNG();
    } else {
        const std::string msg = seed + ":" + std::to_string(i);
        s.setHashOf(msg.data(), msg.size());
    }
    if (s.isZero()) s = Fr(1);
    return s;
}

G1 bn254_explicit_base_point() {
    G1 base;
    base.setStr("1 0x2523648240000001ba344d80000000086121000000000013a700000000000012 0x1");
    if (base.isZero()) {
        throw std::runtime_error("explicit BN254 base point parsed as infinity");
    }
    return base;
}

/*****************scalar digit extraction****************/
uint32_t extract_digit(uint64_t abs_lo, uint64_t abs_hi, int shift, uint32_t mask) {
    if (shift >= 128) return 0;
    if (shift == 0) {
        return static_cast<uint32_t>(abs_lo & mask);
    }
    if (shift < 64) {
        return static_cast<uint32_t>(((abs_lo >> shift) | (abs_hi << (64 - shift))) & mask);
    }
    return static_cast<uint32_t>((abs_hi >> (shift - 64)) & mask);
}

uint32_t extract_digit_u256(const uint64_t *limbs, int shift, uint32_t mask) {
    if (shift >= 256) return 0;
    const int word = shift / 64;
    const int offset = shift % 64;
    uint64_t value = limbs[word] >> offset;
    if (offset != 0 && word + 1 < 4) {
        value |= limbs[word + 1] << (64 - offset);
    }
    return static_cast<uint32_t>(value & mask);
}

int compare_u256(const uint64_t *a, const uint64_t *b) {
    for (int i = 3; i >= 0; --i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

bool is_zero_u256(const uint64_t *a) {
    return (a[0] | a[1] | a[2] | a[3]) == 0;
}

void subtract_u256(const uint64_t *a, const uint64_t *b, uint64_t *out) {
    uint64_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
        const uint64_t bi = b[i] + borrow;
        const uint64_t next_borrow = (borrow == 0) ? (a[i] < b[i]) : (a[i] <= b[i]);
        out[i] = a[i] - bi;
        borrow = next_borrow;
    }
}

// Signed128 非零预解码，跳过的是“整个 scalar 为零”，不是“当前窗口 digit 为零”。
struct NzScalar128 {
    uint64_t abs_lo = 0;
    uint64_t abs_hi = 0;
    uint16_t point_index = 0;
    uint8_t row = 0;
    uint8_t neg = 0;
};

void build_nonzero_signed128_tile(
    const std::vector<uint64_t>& scalar_tile,
    size_t tile_rows,
    size_t n,
    std::vector<NzScalar128>& out,
    uint64_t& zero_count
) {
    if (tile_rows > 255) {
        throw std::runtime_error("tile_rows exceeds uint8_t range for compact signed128 predecode");
    }
    if (n > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("n exceeds uint16_t range for compact signed128 predecode");
    }

    out.clear();
    const size_t total = tile_rows * n;
    const size_t reserve_hint = total / 3;

    for (size_t r = 0; r < tile_rows; ++r) {
        const size_t row_base = r * n * 4;
        for (size_t i = 0; i < n; ++i) {
            const uint64_t* limbs = &scalar_tile[row_base + i * 4];
            const uint64_t lo = limbs[0];
            const uint64_t hi = limbs[1];
            if ((lo | hi) == 0) continue;

            if (out.empty() && out.capacity() < reserve_hint) {
                out.reserve(reserve_hint);
            }

            const bool is_neg = (hi >> 63) != 0;
            uint64_t abs_lo = lo;
            uint64_t abs_hi = hi;
            if (is_neg) {
                abs_lo = ~lo + 1;
                const uint64_t carry = (abs_lo == 0) ? 1 : 0;
                abs_hi = ~hi + carry;
            }

            NzScalar128 ev;
            ev.abs_lo = abs_lo;
            ev.abs_hi = abs_hi;
            ev.point_index = static_cast<uint16_t>(i);
            ev.row = static_cast<uint8_t>(r);
            ev.neg = static_cast<uint8_t>(is_neg ? 1 : 0);
            out.push_back(ev);
        }
    }
    zero_count = static_cast<uint64_t>(total - out.size());
}


// BN254 / BN254-balanced 非零预解码。
// 与 NzScalar128 不同，这里保留完整四个 64-bit limb，避免截断高 128 位。
// magnitude 始终保存绝对值；neg 保存 balanced 表示下的符号。
struct NzScalar256 {
    uint64_t magnitude[4] = {0, 0, 0, 0};
    uint16_t point_index = 0;
    uint8_t row = 0;
    uint8_t neg = 0;
};

void build_nonzero_bn254_tile(
    const std::vector<uint64_t>& scalar_tile,
    size_t tile_rows,
    size_t n,
    ScalarFormatKind format,
    std::vector<NzScalar256>& out,
    uint64_t& zero_count
) {
    if (format == ScalarFormatKind::Signed128) {
        throw std::runtime_error(
            "build_nonzero_bn254_tile does not accept signed128 format");
    }
    if (tile_rows > 255) {
        throw std::runtime_error(
            "tile_rows exceeds uint8_t range for compact BN254 predecode");
    }
    if (n > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error(
            "n exceeds uint16_t range for compact BN254 predecode");
    }

    out.clear();
    const size_t total = tile_rows * n;
    const size_t reserve_hint = total / 3;

    for (size_t r = 0; r < tile_rows; ++r) {
        const size_t row_base = r * n * 4;
        for (size_t i = 0; i < n; ++i) {
            const uint64_t* limbs = &scalar_tile[row_base + i * 4];

            // 必须检查四个 limb。只检查前两个 limb 会把高 128 位非零的
            // BN254 scalar 错误地当成零。
            if (is_zero_u256(limbs)) continue;

            if (out.empty() && out.capacity() < reserve_hint) {
                out.reserve(reserve_hint);
            }

            NzScalar256 ev;
            ev.point_index = static_cast<uint16_t>(i);
            ev.row = static_cast<uint8_t>(r);

            if (format == ScalarFormatKind::Bn254Balanced &&
                compare_u256(limbs, BN254_FR_HALF) > 0) {
                // a > r/2 时使用 -(r-a)，与 scalar_digit_and_sign 的语义一致。
                subtract_u256(BN254_FR_MODULUS, limbs, ev.magnitude);
                ev.neg = 1;
            } else {
                ev.magnitude[0] = limbs[0];
                ev.magnitude[1] = limbs[1];
                ev.magnitude[2] = limbs[2];
                ev.magnitude[3] = limbs[3];
                ev.neg = 0;
            }

            out.push_back(ev);
        }
    }

    zero_count = static_cast<uint64_t>(total - out.size());
}

// Group value used by the active fast64 exact-mask path.
struct MaskPointGroup {
    G1 sum;
    uint32_t count = 0;
    uint64_t row_mask = 0;
};

// One non-zero digit after scalar predecode. The point index already fits in
// 16 bits (the loader enforces n <= 65535), the row needs six bits, and the
// remaining high bits hold the signed-bucket slot. Thus one event is exactly
// eight bytes and digit extraction is paid once per tile, not once per later
// window pass.
struct PackedWindowEvent {
    uint64_t packed = 0;
};
static_assert(
    sizeof(PackedWindowEvent) == sizeof(uint64_t),
    "PackedWindowEvent must remain eight bytes"
);

static inline PackedWindowEvent make_window_event(
    uint16_t point_index,
    uint8_t row,
    size_t slot
) {
    if (slot >= (uint64_t{1} << 42)) {
        throw std::runtime_error("signed bucket slot does not fit packed window event");
    }
    return PackedWindowEvent{
        static_cast<uint64_t>(point_index) |
        (static_cast<uint64_t>(row & 0x3f) << 16) |
        (static_cast<uint64_t>(slot) << 22)
    };
}

static inline uint16_t window_event_point(PackedWindowEvent ev) {
    return static_cast<uint16_t>(ev.packed & 0xffffu);
}

static inline uint8_t window_event_row(PackedWindowEvent ev) {
    return static_cast<uint8_t>((ev.packed >> 16) & 0x3fu);
}

static inline size_t window_event_slot(PackedWindowEvent ev) {
    return static_cast<size_t>(ev.packed >> 22);
}

// Deferred singleton update. target encodes sign and the absolute row-bucket
// index so execution can be counting-scattered without comparison sorting.
struct SingletonExecEvent {
    uint32_t point_index = 0;
    uint32_t target = 0;
};

static constexpr uint32_t SINGLETON_NEGATIVE_FLAG = uint32_t{1} << 31;

static inline SingletonExecEvent make_singleton_exec_event(
    uint32_t point_index,
    size_t row,
    uint32_t q,
    int signed_bucket
) {
    const uint32_t bucket =
        static_cast<uint32_t>(signed_bucket < 0 ? -signed_bucket : signed_bucket);
    const uint32_t row_bucket =
        static_cast<uint32_t>(row * static_cast<size_t>(q) + bucket);
    return SingletonExecEvent{
        point_index,
        row_bucket |
            (signed_bucket < 0 ? SINGLETON_NEGATIVE_FLAG : uint32_t{0})
    };
}

static inline uint32_t singleton_target_bucket(SingletonExecEvent ev) {
    return ev.target & ~SINGLETON_NEGATIVE_FLAG;
}

static inline bool singleton_target_negative(SingletonExecEvent ev) {
    return (ev.target & SINGLETON_NEGATIVE_FLAG) != 0;
}

inline size_t signed_bucket_to_slot(int signed_bucket, uint32_t q);

// One scalar scan performs zero detection, sign/magnitude conversion and all
// window digit extraction. This replaces the v38 scalar -> NzScalar -> event
// two-stage pipeline in the active fast64 path.
void build_window_events_fused(
    const std::vector<uint64_t>& scalar_tile,
    size_t tile_rows,
    size_t n,
    int c,
    int windows,
    uint32_t q,
    uint32_t digit_mask,
    ScalarFormatKind format,
    std::vector<std::vector<PackedWindowEvent>>& window_events,
    uint64_t& nonzero_count,
    uint64_t& zero_count
) {
    nonzero_count = 0;
    zero_count = 0;
    for (size_t r = 0; r < tile_rows; ++r) {
        const size_t row_base = r * n * 4;
        for (size_t point_index = 0; point_index < n; ++point_index) {
            const uint64_t* limbs = &scalar_tile[row_base + point_index * 4];
            uint64_t magnitude[4] = {0, 0, 0, 0};
            bool neg = false;

            if (format == ScalarFormatKind::Signed128) {
                const uint64_t lo = limbs[0];
                const uint64_t hi = limbs[1];
                if ((lo | hi) == 0) {
                    ++zero_count;
                    continue;
                }
                neg = (hi >> 63) != 0;
                magnitude[0] = lo;
                magnitude[1] = hi;
                if (neg) {
                    magnitude[0] = ~lo + 1;
                    const uint64_t carry = magnitude[0] == 0 ? 1 : 0;
                    magnitude[1] = ~hi + carry;
                }
            } else {
                if (is_zero_u256(limbs)) {
                    ++zero_count;
                    continue;
                }
                if (format == ScalarFormatKind::Bn254Balanced &&
                    compare_u256(limbs, BN254_FR_HALF) > 0) {
                    subtract_u256(BN254_FR_MODULUS, limbs, magnitude);
                    neg = true;
                } else {
                    std::memcpy(magnitude, limbs, sizeof(magnitude));
                }
            }

            ++nonzero_count;
            for (int w = 0; w < windows; ++w) {
                const uint32_t d =
                    (format == ScalarFormatKind::Signed128)
                        ? extract_digit(
                              magnitude[0],
                              magnitude[1],
                              w * c,
                              digit_mask)
                        : extract_digit_u256(
                              magnitude,
                              w * c,
                              digit_mask);
                if (d == 0) continue;
                const int signed_bucket =
                    neg ? -static_cast<int>(d) : static_cast<int>(d);
                window_events[static_cast<size_t>(w)].push_back(
                    make_window_event(
                        static_cast<uint16_t>(point_index),
                        static_cast<uint8_t>(r),
                        signed_bucket_to_slot(signed_bucket, q)));
            }
        }
    }
}

size_t popcount64(uint64_t x) {
#if !defined(_MSC_VER)
    return static_cast<size_t>(__builtin_popcountll(x));
#else
    size_t n = 0;
    while (x != 0) {
        x &= x - 1;
        ++n;
    }
    return n;
#endif
}

size_t ctz64_portable(uint64_t x) {
#if !defined(_MSC_VER)
    return static_cast<size_t>(__builtin_ctzll(x));
#else
    size_t n = 0;
    while ((x & 1) == 0) {
        x >>= 1;
        ++n;
    }
    return n;
#endif
}

// Tiny flat open-addressing map used for both counts and compact execution
// metadata. G1 values live in a separate dense array, so empty hash slots never
// default-construct curve points.
template <typename Value>
class FlatHashMap64 {
public:
    struct Entry {
        uint64_t first = 0;
        Value second{};
        bool used = false;
    };

    class iterator {
    public:
        iterator() = default;
        iterator(std::vector<Entry>* slots, size_t index) : slots_(slots), index_(index) {
            skip_unused();
        }
        Entry& operator*() const { return (*slots_)[index_]; }
        Entry* operator->() const { return &(*slots_)[index_]; }
        iterator& operator++() {
            ++index_;
            skip_unused();
            return *this;
        }
        bool operator==(const iterator& other) const {
            return slots_ == other.slots_ && index_ == other.index_;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }
    private:
        void skip_unused() {
            if (!slots_) return;
            while (index_ < slots_->size() && !(*slots_)[index_].used) ++index_;
        }
        std::vector<Entry>* slots_ = nullptr;
        size_t index_ = 0;
    };

    class const_iterator {
    public:
        const_iterator() = default;
        const_iterator(const std::vector<Entry>* slots, size_t index) : slots_(slots), index_(index) {
            skip_unused();
        }
        const Entry& operator*() const { return (*slots_)[index_]; }
        const Entry* operator->() const { return &(*slots_)[index_]; }
        const_iterator& operator++() {
            ++index_;
            skip_unused();
            return *this;
        }
        bool operator==(const const_iterator& other) const {
            return slots_ == other.slots_ && index_ == other.index_;
        }
        bool operator!=(const const_iterator& other) const { return !(*this == other); }
    private:
        void skip_unused() {
            if (!slots_) return;
            while (index_ < slots_->size() && !(*slots_)[index_].used) ++index_;
        }
        const std::vector<Entry>* slots_ = nullptr;
        size_t index_ = 0;
    };

    FlatHashMap64() = default;

    void reserve(size_t requested) {
        const size_t need = requested * 2 + 8;
        if (need > slots_.size()) {
            rehash(next_power_of_two(need));
        }
    }

    void clear() {
        for (Entry& e : slots_) e.used = false;
        size_ = 0;
    }

    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }

    Value& operator[](uint64_t key) {
        ensure_insert_capacity();
        return find_or_insert(key);
    }

    iterator find(uint64_t key) {
        if (slots_.empty()) return end();
        const size_t mask = slots_.size() - 1;
        size_t idx = hash64(key) & mask;
        while (slots_[idx].used) {
            if (slots_[idx].first == key) return iterator(&slots_, idx);
            idx = (idx + 1) & mask;
        }
        return end();
    }

    const_iterator find(uint64_t key) const {
        if (slots_.empty()) return end();
        const size_t mask = slots_.size() - 1;
        size_t idx = hash64(key) & mask;
        while (slots_[idx].used) {
            if (slots_[idx].first == key) return const_iterator(&slots_, idx);
            idx = (idx + 1) & mask;
        }
        return end();
    }

    iterator begin() { return iterator(&slots_, 0); }
    iterator end() { return iterator(&slots_, slots_.size()); }
    const_iterator begin() const { return const_iterator(&slots_, 0); }
    const_iterator end() const { return const_iterator(&slots_, slots_.size()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

private:
    static size_t next_power_of_two(size_t x) {
        size_t p = 8;
        while (p < x) p <<= 1;
        return p;
    }

    static size_t hash64(uint64_t x) {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        x ^= (x >> 31);
        return static_cast<size_t>(x);
    }

    void ensure_insert_capacity() {
        if (slots_.empty()) {
            rehash(8);
            return;
        }
        // Keep load factor <= 0.5. This is intentionally lower than std::unordered_map
        // because linear probing is fastest when probe chains stay short.
        if ((size_ + 1) * 2 >= slots_.size()) {
            rehash(slots_.size() * 2);
        }
    }

    Value& find_or_insert(uint64_t key) {
        const size_t mask = slots_.size() - 1;
        size_t idx = hash64(key) & mask;
        while (slots_[idx].used) {
            if (slots_[idx].first == key) return slots_[idx].second;
            idx = (idx + 1) & mask;
        }
        Entry& e = slots_[idx];
        e.first = key;
        e.second = Value{};
        e.used = true;
        ++size_;
        return e.second;
    }

    void rehash(size_t new_capacity) {
        new_capacity = next_power_of_two(new_capacity);
        std::vector<Entry> old;
        old.swap(slots_);
        slots_.assign(new_capacity, Entry{});
        size_ = 0;
        for (Entry& e : old) {
            if (!e.used) continue;
            Value& dst = find_or_insert(e.first);
            dst = std::move(e.second);
        }
    }

    std::vector<Entry> slots_;
    size_t size_ = 0;
};

using CountMap64 = FlatHashMap64<uint32_t>;
using BucketCountMap64 = std::unordered_map<int, CountMap64>;

static constexpr uint32_t FAST64_NO_GROUP = std::numeric_limits<uint32_t>::max();

struct MaskExecMeta64 {
    uint32_t count = 0;
    uint32_t group_index = FAST64_NO_GROUP;
};

// Per-leaf fused metadata table. It replaces the old pair
// (shareable_rows flat map, shared_groups unordered_map). Every mask is looked
// up once and a shareable mask directly names its dense G1 accumulator.
using MaskExecMap64 = FlatHashMap64<MaskExecMeta64>;

inline uint64_t exact_mask_gain64(uint32_t count, uint64_t row_mask) {
    const uint64_t u = static_cast<uint64_t>(popcount64(row_mask));
    if (count == 0 || u == 0) return 0;
    return (static_cast<uint64_t>(count) - 1) * (u - 1);
}

inline uint64_t raw_mask_cost64(uint32_t count, uint64_t row_mask) {
    return static_cast<uint64_t>(count) * static_cast<uint64_t>(popcount64(row_mask));
}

inline uint64_t shared_mask_cost64(uint32_t count, uint64_t row_mask) {
    if (count == 0) return 0;
    return (static_cast<uint64_t>(count) - 1) + static_cast<uint64_t>(popcount64(row_mask));
}

inline bool should_share_mask64(uint32_t count, uint64_t row_mask, const Options& opt) {
    if (count < opt.fast_min_group_count) return false;
    if (popcount64(row_mask) < 2) return false;
    return exact_mask_gain64(count, row_mask) >= opt.fast_min_gain;
}

void add_point_to_row_buckets64(
    std::vector<G1>& row_buckets,
    size_t q,
    int signed_bucket,
    uint64_t row_mask,
    const G1& point
) {
    const uint32_t bucket_id = static_cast<uint32_t>(signed_bucket < 0 ? -signed_bucket : signed_bucket);
    uint64_t rows = row_mask;
    while (rows != 0) {
        const size_t r = ctz64_portable(rows);
        if (signed_bucket < 0) {
            row_buckets[r * q + bucket_id] -= point;
        } else {
            row_buckets[r * q + bucket_id] += point;
        }
        rows &= rows - 1;
    }
}

void add_group_to_row_buckets64(
    std::vector<G1>& row_buckets,
    size_t q,
    int signed_bucket,
    uint64_t row_mask,
    const G1& group_sum
) {
    const uint32_t bucket_id = static_cast<uint32_t>(signed_bucket < 0 ? -signed_bucket : signed_bucket);
    uint64_t rows = row_mask;
    while (rows != 0) {
        const size_t r = ctz64_portable(rows);
        if (signed_bucket < 0) {
            row_buckets[r * q + bucket_id] -= group_sum;
        } else {
            row_buckets[r * q + bucket_id] += group_sum;
        }
        rows &= rows - 1;
    }
}

inline void add_point_to_single_row_bucket64(
    std::vector<G1>& row_buckets,
    size_t q,
    int signed_bucket,
    size_t row,
    const G1& point
) {
    const uint32_t bucket_id = static_cast<uint32_t>(signed_bucket < 0 ? -signed_bucket : signed_bucket);
    if (signed_bucket < 0) {
        row_buckets[row * q + bucket_id] -= point;
    } else {
        row_buckets[row * q + bucket_id] += point;
    }
}

// Execute deferred singleton updates. Small batches retain insertion order.
// Large batches use O(events + row_buckets) counting scatter so consecutive
// additions update the same row bucket and keep its G1 accumulator hot.
void execute_singleton_events_adaptive(
    std::vector<G1>& row_buckets,
    const std::vector<G1>& points,
    uint32_t q,
    std::vector<SingletonExecEvent>& events,
    std::vector<SingletonExecEvent>& scatter,
    std::vector<uint32_t>& counts,
    std::vector<uint32_t>& cursors
) {
    constexpr size_t kScatterThreshold = 4096;
    if (events.size() < kScatterThreshold) {
        for (SingletonExecEvent ev : events) {
            G1& dst = row_buckets[singleton_target_bucket(ev)];
            if (singleton_target_negative(ev)) dst -= points[ev.point_index];
            else dst += points[ev.point_index];
        }
        return;
    }

    const size_t target_count = row_buckets.size() * 2;
    counts.assign(target_count, 0);
    cursors.assign(points.size(), 0);
    for (SingletonExecEvent ev : events) {
        const size_t target =
            static_cast<size_t>(singleton_target_bucket(ev)) +
            (singleton_target_negative(ev) ? row_buckets.size() : 0);
        ++counts[target];
        ++cursors[ev.point_index];
    }

    size_t unique_points = 0;
    for (uint32_t count : cursors) unique_points += count != 0;
    // When each loaded point feeds many destinations, point-major execution
    // wins by reusing the 144-byte G1 value. Otherwise target-major execution
    // keeps the row-bucket accumulator hot.
    const bool use_point_major =
        unique_points != 0 && events.size() >= unique_points * 3;
    if (use_point_major) {
        uint32_t prefix = 0;
        for (size_t point = 0; point < cursors.size(); ++point) {
            const uint32_t count = cursors[point];
            cursors[point] = prefix;
            prefix += count;
        }
        scatter.resize(events.size());
        for (SingletonExecEvent ev : events) {
            scatter[cursors[ev.point_index]++] = ev;
        }
        size_t begin = 0;
        for (size_t point = 0; point < points.size(); ++point) {
            const size_t end = cursors[point];
            const G1& value = points[point];
            for (size_t i = begin; i < end; ++i) {
                G1& dst = row_buckets[singleton_target_bucket(scatter[i])];
                if (singleton_target_negative(scatter[i])) dst -= value;
                else dst += value;
            }
            begin = end;
        }
        return;
    }

    cursors.resize(target_count);
    uint32_t prefix = 0;
    for (size_t target = 0; target < target_count; ++target) {
        cursors[target] = prefix;
        prefix += counts[target];
    }
    scatter.resize(events.size());
    for (SingletonExecEvent ev : events) {
        const size_t target =
            static_cast<size_t>(singleton_target_bucket(ev)) +
            (singleton_target_negative(ev) ? row_buckets.size() : 0);
        scatter[cursors[target]++] = ev;
    }

    size_t begin = 0;
    for (size_t target = 0; target < target_count; ++target) {
        const size_t end = begin + counts[target];
        if (begin != end) {
            G1& dst = row_buckets[target % row_buckets.size()];
            if (target >= row_buckets.size()) {
                for (size_t i = begin; i < end; ++i) {
                    dst -= points[scatter[i].point_index];
                }
            } else {
                for (size_t i = begin; i < end; ++i) {
                    dst += points[scatter[i].point_index];
                }
            }
        }
        begin = end;
    }
    (void)q;
}

// v13: faster fallback path for a row mask with exactly one set bit.
// This avoids entering add_point_to_row_buckets64's while-loop for the common
// non-shareable single-row case. It returns the number of row-bucket updates.
inline uint64_t add_point_to_row_buckets64_fallback_fast(
    std::vector<G1>& row_buckets,
    size_t q,
    int signed_bucket,
    uint64_t row_mask,
    const G1& point,
    TileMaskStats& stats
) {
    if (row_mask == 0) return 0;
    const bool single_row = (row_mask & (row_mask - 1)) == 0;
    if (single_row) {
        const size_t r = ctz64_portable(row_mask);
        add_point_to_single_row_bucket64(row_buckets, q, signed_bucket, r, point);
        ++stats.fallback_popcount1_events;
        return 1;
    }
    add_point_to_row_buckets64(row_buckets, q, signed_bucket, row_mask, point);
    return static_cast<uint64_t>(popcount64(row_mask));
}

inline size_t signed_bucket_to_slot(int signed_bucket, uint32_t q) {
    const uint32_t abs_bucket = static_cast<uint32_t>(signed_bucket < 0 ? -signed_bucket : signed_bucket);
    if (signed_bucket > 0) return static_cast<size_t>(abs_bucket - 1);
    return static_cast<size_t>((q - 1) + (abs_bucket - 1));
}

inline int slot_to_signed_bucket(size_t slot, uint32_t q) {
    if (slot < static_cast<size_t>(q - 1)) {
        return static_cast<int>(slot + 1);
    }
    return -static_cast<int>(slot - static_cast<size_t>(q - 1) + 1);
}


inline uint64_t row_range_mask64(size_t begin, size_t end) {
    if (begin >= end) return 0;
    if (begin >= 64) return 0;
    if (end > 64) end = 64;
    uint64_t lo = std::numeric_limits<uint64_t>::max();
    if (begin > 0) lo <<= begin;
    uint64_t hi = std::numeric_limits<uint64_t>::max();
    if (end < 64) hi = (uint64_t{1} << end) - 1;
    return lo & hi;
}

struct Fast64LeafPlan {
    size_t begin = 0;
    size_t end = 0;
    uint64_t range_mask = 0; // cached row_range_mask64(begin, end), avoids recomputing in group_event.
    CountMap64 counts; // masks stay in the original tile-local 0..63 bit coordinates.
};

// Two-stage first-hit representation.
//
// Original Fast64FirstHit was {uint32_t point_index; uint8_t first_row;},
// which is normally padded to 8 bytes. Since tile_rows <= 64, first_row only
// needs 6 bits, so we pack:
//   low  6 bits = first_row
//   high bits   = point_index
//
// A singleton never receives a 64-bit row mask. first_hit_state stores either:
//   0                         : unseen
//   1..64                     : first row + 1
//   FAST64_PROMOTED_FLAG | i  : index i in the slot's promoted arrays
//
// Only a second hit in a different row promotes the point and allocates a
// uint64_t mask in promoted_masks_by_slot. This avoids writing/scanning a dense
// 64-bit mask workspace for the overwhelmingly common singleton case.
struct Fast64FirstHit {
    uint32_t packed = 0;
};

static inline Fast64FirstHit make_fast64_first_hit(uint32_t point_index, uint8_t first_row) {
    return Fast64FirstHit{(point_index << 6) | static_cast<uint32_t>(first_row & 0x3f)};
}

static inline uint32_t fast64_first_hit_point(Fast64FirstHit hit) {
    return hit.packed >> 6;
}

static inline uint8_t fast64_first_hit_row(Fast64FirstHit hit) {
    return static_cast<uint8_t>(hit.packed & 0x3f);
}

static constexpr uint32_t FAST64_PROMOTED_FLAG = uint32_t{1} << 31;
static constexpr uint32_t FAST64_PROMOTED_INDEX_MASK = ~FAST64_PROMOTED_FLAG;

static inline bool fast64_state_is_promoted(uint32_t state) {
    return (state & FAST64_PROMOTED_FLAG) != 0;
}

static inline uint32_t fast64_promoted_index(uint32_t state) {
    return state & FAST64_PROMOTED_INDEX_MASK;
}

// Per-worker reusable workspace for the fast64 path.
struct Fast64ThreadWorkspace {
    std::vector<G1> row_buckets;
    // Dense 32-bit lookup state; the 64-bit masks themselves are sparse and
    // exist only for promoted (multi-row) points.
    std::vector<uint32_t> first_hit_state;
    std::vector<std::vector<uint32_t>> touched_points;
    std::vector<std::vector<uint64_t>> promoted_masks_by_slot;
    std::vector<size_t> touched_slots;
    std::vector<uint8_t> slot_touched;
    std::vector<CountMap64> counts_by_slot;
    std::vector<std::vector<Fast64LeafPlan>> leaf_plans_by_slot;
    std::vector<std::vector<Fast64FirstHit>> first_hits_by_slot;
    std::vector<std::vector<PackedWindowEvent>> window_events;
    MaskExecMap64 mask_exec_table;
    std::vector<MaskPointGroup> dense_groups;
    std::vector<SingletonExecEvent> singleton_events;
    std::vector<SingletonExecEvent> singleton_scatter;
    std::vector<uint32_t> singleton_target_counts;
    std::vector<uint32_t> singleton_target_cursors;
    std::vector<uint32_t> group_offsets;
    std::vector<uint32_t> group_cursors;
    std::vector<uint32_t> group_point_indices;
    std::vector<NzScalar128> nz_scalars128;
    std::vector<NzScalar256> nz_scalars256;

    void prepare(
        size_t tile_rows,
        size_t n,
        uint32_t q,
        size_t signed_bucket_slot_count,
        size_t windows
    ) {
        const size_t row_bucket_size = tile_rows * static_cast<size_t>(q);
        if (row_buckets.size() != row_bucket_size) {
            row_buckets.resize(row_bucket_size);
        }

        const size_t state_size = signed_bucket_slot_count * n;
        if (first_hit_state.size() != state_size) {
            // Initialized once per worker/shape. The hot path resets only
            // entries listed in first_hits_by_slot.
            first_hit_state.assign(state_size, 0);
        }

        if (touched_points.size() != signed_bucket_slot_count) {
            touched_points.assign(signed_bucket_slot_count, std::vector<uint32_t>());
        }
        if (promoted_masks_by_slot.size() != signed_bucket_slot_count) {
            promoted_masks_by_slot.assign(
                signed_bucket_slot_count,
                std::vector<uint64_t>()
            );
        }
        touched_slots.clear();
        if (slot_touched.size() != signed_bucket_slot_count) {
            slot_touched.assign(signed_bucket_slot_count, 0);
        } else {
            std::fill(slot_touched.begin(), slot_touched.end(), uint8_t{0});
        }
        if (counts_by_slot.size() != signed_bucket_slot_count) {
            counts_by_slot.assign(signed_bucket_slot_count, CountMap64());
        }
        if (leaf_plans_by_slot.size() != signed_bucket_slot_count) {
            leaf_plans_by_slot.assign(signed_bucket_slot_count, std::vector<Fast64LeafPlan>());
        }
        if (first_hits_by_slot.size() != signed_bucket_slot_count) {
            first_hits_by_slot.assign(signed_bucket_slot_count, std::vector<Fast64FirstHit>());
        }
        if (window_events.size() != windows) {
            window_events.resize(windows);
        }
        for (std::vector<PackedWindowEvent>& events : window_events) {
            events.clear();
        }
        singleton_events.clear();
        singleton_scatter.clear();
        group_offsets.clear();
        group_cursors.clear();
        group_point_indices.clear();
    }
};

struct TileDirectPippengerThreadWorkspace {
    std::vector<G1> row_buckets;
    std::vector<NzScalar128> nz_scalars128;
    std::vector<NzScalar256> nz_scalars256;

    void prepare(size_t tile_rows, uint32_t q) {
        const size_t row_bucket_size = tile_rows * static_cast<size_t>(q);
        if (row_buckets.size() != row_bucket_size) {
            row_buckets.resize(row_bucket_size);
        }
    }
};

uint64_t raw_count_cost64(const CountMap64& counts) {
    uint64_t cost = 0;
    for (const auto& item : counts) {
        cost += raw_mask_cost64(item.second, item.first);
    }
    return cost;
}

uint64_t exact_count_cost64(const CountMap64& counts, const Options& opt) {
    uint64_t cost = 0;
    for (const auto& item : counts) {
        const uint64_t rows = item.first;
        const uint32_t count = item.second;
        if (should_share_mask64(count, rows, opt)) {
            cost += shared_mask_cost64(count, rows);
        } else {
            cost += raw_mask_cost64(count, rows);
        }
    }
    return cost;
}

void exact_count_cost_stats64(
    const CountMap64& counts,
    const Options& opt,
    uint64_t& opt_cost,
    uint64_t& shared_points
) {
    for (const auto& item : counts) {
        const uint64_t rows = item.first;
        const uint32_t count = item.second;
        if (should_share_mask64(count, rows, opt)) {
            opt_cost += shared_mask_cost64(count, rows);
            shared_points += count;
        } else {
            opt_cost += raw_mask_cost64(count, rows);
        }
    }
}

struct CountMetrics64 {
    uint64_t opt_cost = 0;
    uint64_t shared_points = 0;
    uint64_t shareable_groups = 0;
    uint64_t row_links = 0;
    uint64_t execution_events = 0;
};

CountMetrics64 exact_count_metrics64(const CountMap64& counts, const Options& opt) {
    CountMetrics64 m;
    for (const auto& item : counts) {
        const uint64_t rows = item.first;
        const uint32_t count = item.second;
        m.execution_events += count;
        if (should_share_mask64(count, rows, opt)) {
            m.opt_cost += shared_mask_cost64(count, rows);
            m.shared_points += count;
            ++m.shareable_groups;
            m.row_links += static_cast<uint64_t>(popcount64(rows));
            ++m.execution_events; // one shared group writeback
        } else {
            m.opt_cost += raw_mask_cost64(count, rows);
        }
    }
    return m;
}

std::vector<CountMap64> project_counts_to_children64(
    const CountMap64& counts,
    size_t begin,
    size_t end,
    size_t child_tile_size
) {
    const size_t child_count = (end - begin + child_tile_size - 1) / child_tile_size;
    std::vector<CountMap64> children(child_count);
    for (const auto& item : counts) {
        const uint64_t rows = item.first;
        const uint32_t count = item.second;
        for (size_t child_id = 0; child_id < child_count; ++child_id) {
            const size_t child_begin = begin + child_id * child_tile_size;
            const size_t child_end = std::min(end, child_begin + child_tile_size);
            const uint64_t child_rows = rows & row_range_mask64(child_begin, child_end);
            if (child_rows == 0) continue;
            children[child_id][child_rows] += count;
        }
    }
    return children;
}

// Estimate the cost of the selected multiscale plan using no-split exact-mask costs only.
// This mirrors compare_nosplit_refine: compare parent exact-mask cost against
// recursively selected child exact-mask costs, and keep the cheaper scale.
uint64_t estimate_multiscale_count_cost64(
    const CountMap64& counts,
    size_t begin,
    size_t end,
    const Options& opt,
    bool enable_multiscale
) {
    const uint64_t parent_cost = exact_count_cost64(counts, opt);
    const size_t tile_size = end - begin;
    if (!enable_multiscale || tile_size <= opt.split_base_tile_size || counts.empty()) {
        return parent_cost;
    }

    const size_t child_tile_size = std::max(opt.split_base_tile_size, (tile_size + opt.split_factor - 1) / opt.split_factor);
    if (child_tile_size >= tile_size) return parent_cost;

    auto children = project_counts_to_children64(counts, begin, end, child_tile_size);
    uint64_t child_cost = 0;
    for (size_t child_id = 0; child_id < children.size(); ++child_id) {
        if (children[child_id].empty()) continue;
        const size_t child_begin = begin + child_id * child_tile_size;
        const size_t child_end = std::min(end, child_begin + child_tile_size);
        child_cost += estimate_multiscale_count_cost64(children[child_id], child_begin, child_end, opt, enable_multiscale);
    }
    return std::min(parent_cost, child_cost);
}

// Select final leaf ranges for one fast64 bucket. It is deliberately selective:
// parent is kept on ties, and children are used only when their recursively
// selected no-split exact-mask cost is strictly lower than the parent cost.
void select_multiscale_leaves_by_count64(
    CountMap64&& counts,
    size_t begin,
    size_t end,
    const Options& opt,
    bool enable_multiscale,
    int window,
    std::vector<Fast64LeafPlan>& leaves,
    TileMaskStats& stats
) {
    const size_t tile_size = end - begin;
    if (!enable_multiscale || tile_size <= opt.split_base_tile_size || counts.empty()) {
        Fast64LeafPlan leaf;
        leaf.begin = begin;
        leaf.end = end;
        leaf.range_mask = row_range_mask64(begin, end);
        leaf.counts = std::move(counts);
        leaves.push_back(std::move(leaf));
        if (enable_multiscale) ++stats.multiscale_parent_chosen;
        return;
    }

    const uint64_t parent_cost = exact_count_cost64(counts, opt);
    const size_t child_tile_size = std::max(opt.split_base_tile_size, (tile_size + opt.split_factor - 1) / opt.split_factor);
    if (child_tile_size >= tile_size) {
        Fast64LeafPlan leaf;
        leaf.begin = begin;
        leaf.end = end;
        leaf.range_mask = row_range_mask64(begin, end);
        leaf.counts = std::move(counts);
        leaves.push_back(std::move(leaf));
        ++stats.multiscale_parent_chosen;
        return;
    }

    auto children = project_counts_to_children64(counts, begin, end, child_tile_size);
    uint64_t child_cost = 0;
    for (size_t child_id = 0; child_id < children.size(); ++child_id) {
        if (children[child_id].empty()) continue;
        const size_t child_begin = begin + child_id * child_tile_size;
        const size_t child_end = std::min(end, child_begin + child_tile_size);
        child_cost += estimate_multiscale_count_cost64(children[child_id], child_begin, child_end, opt, enable_multiscale);
    }

    const uint64_t extra_saving = parent_cost > child_cost ? parent_cost - child_cost : 0;
    CountMetrics64 child_metrics;
    for (const CountMap64& child_counts : children) {
        CountMetrics64 m = exact_count_metrics64(child_counts, opt);
        child_metrics.opt_cost += m.opt_cost;
        child_metrics.shared_points += m.shared_points;
        child_metrics.shareable_groups += m.shareable_groups;
        child_metrics.row_links += m.row_links;
        child_metrics.execution_events += m.execution_events;
    }

    const uint64_t min_extra_saving = (window == 0) ? 128 : 64;
    const bool low_benefit_child =
        (parent_cost <= child_cost) ||
        (extra_saving < min_extra_saving) ||
        (child_metrics.execution_events > 0 && extra_saving < child_metrics.execution_events) ||
        (child_metrics.row_links > 0 && extra_saving < 2 * child_metrics.row_links);

    if (low_benefit_child) {
        Fast64LeafPlan leaf;
        leaf.begin = begin;
        leaf.end = end;
        leaf.range_mask = row_range_mask64(begin, end);
        leaf.counts = std::move(counts);
        leaves.push_back(std::move(leaf));
        ++stats.multiscale_parent_chosen;
        return;
    }

    ++stats.multiscale_child_chosen;
    for (size_t child_id = 0; child_id < children.size(); ++child_id) {
        if (children[child_id].empty()) continue;
        const size_t child_begin = begin + child_id * child_tile_size;
        const size_t child_end = std::min(end, child_begin + child_tile_size);
        select_multiscale_leaves_by_count64(
            std::move(children[child_id]),
            child_begin,
            child_end,
            opt,
            enable_multiscale,
            window,
            leaves,
            stats
        );
    }
}
// 主函数
TileMaskStats pippenger_tile_mask_fast64_rows(
    std::vector<G1>& row_outputs,
    const std::vector<G1>& points,
    const std::vector<uint64_t>& scalar_tile,
    size_t tile_rows,
    size_t n,
    int c,
    const Options& opt,
    Fast64ThreadWorkspace* reusable_workspace = nullptr
) {
    if (tile_rows > 64) {
        throw std::runtime_error("pippenger_tile_mask_fast64_rows requires tile_rows <= 64");
    }

    const uint32_t q = uint32_t{1} << c;
    const uint32_t digit_mask = q - 1;
    const int windows = (opt.scalar_bits + c - 1) / c;
    const size_t signed_bucket_slot_count = static_cast<size_t>(2 * (q - 1));
    TileMaskStats stats;
    /*****初始化操作*******/
    row_outputs.assign(tile_rows, G1());
    for (size_t r = 0; r < tile_rows; ++r) row_outputs[r].clear();

    /********融合 scalar 解码、非零统计和窗口事件生成******/
    Fast64ThreadWorkspace local_workspace;
    Fast64ThreadWorkspace& ws =
        reusable_workspace ? *reusable_workspace : local_workspace;

    const auto t_fast64_workspace_init0 = std::chrono::steady_clock::now();
    ws.prepare(
        tile_rows,
        n,
        q,
        signed_bucket_slot_count,
        static_cast<size_t>(windows)
    );
    const auto t_fast64_workspace_init1 = std::chrono::steady_clock::now();
    stats.fast64_workspace_init_seconds += elapsed_seconds(t_fast64_workspace_init0, t_fast64_workspace_init1);

    std::vector<G1>& row_buckets = ws.row_buckets;
    std::vector<uint32_t>& first_hit_state = ws.first_hit_state;
    std::vector<std::vector<uint32_t>>& touched_points = ws.touched_points;
    std::vector<std::vector<uint64_t>>& promoted_masks_by_slot =
        ws.promoted_masks_by_slot;
    std::vector<size_t>& touched_slots = ws.touched_slots;
    std::vector<uint8_t>& slot_touched = ws.slot_touched;
    std::vector<CountMap64>& counts_by_slot = ws.counts_by_slot;
    std::vector<std::vector<Fast64LeafPlan>>& leaf_plans_by_slot = ws.leaf_plans_by_slot;
    std::vector<std::vector<Fast64FirstHit>>& first_hits_by_slot = ws.first_hits_by_slot;
    std::vector<std::vector<PackedWindowEvent>>& window_events = ws.window_events;
    MaskExecMap64& mask_exec_table = ws.mask_exec_table;
    std::vector<MaskPointGroup>& dense_groups = ws.dense_groups;
    std::vector<SingletonExecEvent>& singleton_events = ws.singleton_events;
    std::vector<SingletonExecEvent>& singleton_scatter = ws.singleton_scatter;
    std::vector<uint32_t>& singleton_target_counts = ws.singleton_target_counts;
    std::vector<uint32_t>& singleton_target_cursors = ws.singleton_target_cursors;
    std::vector<uint32_t>& group_offsets = ws.group_offsets;
    std::vector<uint32_t>& group_cursors = ws.group_cursors;
    std::vector<uint32_t>& group_point_indices = ws.group_point_indices;

    uint64_t zero_count = 0;
    uint64_t nonzero_count = 0;
    const auto t_event_precompute0 = std::chrono::steady_clock::now();
    build_window_events_fused(
        scalar_tile,
        tile_rows,
        n,
        c,
        windows,
        q,
        digit_mask,
        opt.scalar_format_kind,
        window_events,
        nonzero_count,
        zero_count);
    const auto t_event_precompute1 = std::chrono::steady_clock::now();
    stats.event_precompute_seconds +=
        elapsed_seconds(t_event_precompute0, t_event_precompute1);
    ++stats.scalar_predecode_tiles;
    stats.scalar_predecode_nonzero_scalars += nonzero_count;
    stats.scalar_predecode_zero_scalars += zero_count;
    stats.scalar_predecode_capacity += 0;

    if (nonzero_count == 0) {
        ++stats.zero_tile_skipped_count;
        return stats;
    }

    /**********内存估计**************/
    const uint64_t row_buckets_bytes =
        static_cast<uint64_t>(row_buckets.capacity()) * static_cast<uint64_t>(sizeof(G1));
    const uint64_t first_hit_state_bytes =
        static_cast<uint64_t>(first_hit_state.capacity()) *
        static_cast<uint64_t>(sizeof(uint32_t));
    const uint64_t touched_points_outer_bytes =
        static_cast<uint64_t>(touched_points.capacity()) * static_cast<uint64_t>(sizeof(std::vector<uint32_t>));
    const uint64_t promoted_masks_outer_bytes =
        static_cast<uint64_t>(promoted_masks_by_slot.capacity()) *
        static_cast<uint64_t>(sizeof(std::vector<uint64_t>));
    const uint64_t touched_slots_bytes =
        static_cast<uint64_t>(touched_slots.capacity()) * static_cast<uint64_t>(sizeof(size_t));
    const uint64_t slot_touched_bytes =
        static_cast<uint64_t>(slot_touched.capacity()) * static_cast<uint64_t>(sizeof(uint8_t));
    const uint64_t counts_outer_bytes =
        static_cast<uint64_t>(counts_by_slot.capacity()) * static_cast<uint64_t>(sizeof(CountMap64));
    const uint64_t leaf_outer_bytes =
        static_cast<uint64_t>(leaf_plans_by_slot.capacity()) * static_cast<uint64_t>(sizeof(std::vector<Fast64LeafPlan>));
    const uint64_t static_workspace_bytes =
        row_buckets_bytes +
        first_hit_state_bytes +
        touched_points_outer_bytes +
        promoted_masks_outer_bytes +
        touched_slots_bytes +
        slot_touched_bytes +
        counts_outer_bytes +
        leaf_outer_bytes;

    uint64_t window_events_bytes =
        static_cast<uint64_t>(window_events.capacity()) *
        static_cast<uint64_t>(sizeof(std::vector<PackedWindowEvent>));
    for (const std::vector<PackedWindowEvent>& events : window_events) {
        window_events_bytes +=
            static_cast<uint64_t>(events.capacity()) *
            static_cast<uint64_t>(sizeof(PackedWindowEvent));
    }

    stats.memory_fast64_row_buckets_max_bytes = row_buckets_bytes;
    // Keep the existing JSON field name for compatibility. It now reports the
    // dense 32-bit first-hit lookup plus sparse promoted 64-bit masks.
    stats.memory_fast64_bucket_point_masks_max_bytes = first_hit_state_bytes;
    stats.memory_fast64_static_workspace_max_bytes = static_workspace_bytes;
    // The active fast64 path no longer materializes NzScalar arrays.
    const uint64_t nz_scalars_bytes = 0;
    stats.memory_fast64_nz_scalars_max_bytes = nz_scalars_bytes;
    stats.memory_fast64_estimated_peak_tile_max_bytes =
        static_workspace_bytes + nz_scalars_bytes + window_events_bytes;
    stats.memory_fast64_window_events_max_bytes = window_events_bytes;

    /**************开始计算**************/
    bool have_window = false;
    for (int w = windows - 1; w >= 0; --w) {
        // 初始化统计数组
        if (have_window) {
            for (int bit = 0; bit < c; ++bit) {
                for (size_t r = 0; r < tile_rows; ++r) row_outputs[r] += row_outputs[r];
            }
        }
        have_window = true;
        const auto t_workspace_clear0 = std::chrono::steady_clock::now();
        for (G1& bucket : row_buckets) bucket.clear();
        const auto t_workspace_clear1 = std::chrono::steady_clock::now();
        stats.tile_workspace_clear_seconds += elapsed_seconds(t_workspace_clear0, t_workspace_clear1);
        const std::vector<PackedWindowEvent>& events =
            window_events[static_cast<size_t>(w)];

        /*========不是热点窗口=========*/
        if (!fast64_enabled_for_window(opt, w)) {
            const auto t_direct0 = std::chrono::steady_clock::now();
            uint64_t direct_adds = 0;
            for (PackedWindowEvent ev : events) {
                const size_t slot = window_event_slot(ev);
                add_point_to_single_row_bucket64(
                    row_buckets,
                    q,
                    slot_to_signed_bucket(slot, q),
                    static_cast<size_t>(window_event_row(ev)),
                    points[window_event_point(ev)]
                );
                ++direct_adds;
            }

            stats.raw_row_bucket_adds += direct_adds;
            stats.optimized_bucket_adds_est += direct_adds;
            stats.fast_direct_row_bucket_adds += direct_adds;
            const auto t_direct1 = std::chrono::steady_clock::now();
            const double direct_seconds = elapsed_seconds(t_direct0, t_direct1);
            stats.g1_group_build_seconds += direct_seconds;
            stats.direct_window_seconds += direct_seconds;
        } 
        
        /*========热点窗口=========*/
        else {
            // v18: scan scalar digits with slot-grouped first-hit singleton suppression.
            // For a given (signed bucket slot, fixed point), the first row hit is
            // stored in first_hits_by_slot[slot] together with its first row. Only when the same point appears
            // again in another row do we promote it into touched_points[slot].
            //
            // Difference from v17:
            //   v17 used one global first_hits vector and accumulated singletons in
            //   row-major scan order, which made row_buckets writes very scattered.
            //   v18 accumulates confirmed singleton points by slot/bucket order, so
            //   direct singleton bucket updates keep the locality of the old v16
            //   finalize-by-slot path while still avoiding the huge singleton counts path.
            const auto t_count0 = std::chrono::steady_clock::now();
            const auto t_scan0 = t_count0;

            /*============================scan阶段========================================*/
            //在当前 window 下，扫描一个 tile 内所有非零 scalar digit
            //把每个固定点 point_index 在某个 signed bucket 中出现过的 MSM 行集合记录成一个 64-bit mask
            //后面再根据这个 mask 判断哪些点可以跨 MSM 共享点加
            const auto record_hit = [&](size_t slot, uint32_t point_index, uint8_t row) {
                const size_t state_index =
                    slot * n + static_cast<size_t>(point_index);
                uint32_t& state = first_hit_state[state_index];
                const uint64_t row_bit = uint64_t{1} << row;

                if (state == 0) {
                    // First hit: record only six bits of row information. No
                    // uint64_t mask is allocated or written for this point.
                    state = static_cast<uint32_t>(row) + 1;
                    if (!slot_touched[slot]) {
                        slot_touched[slot] = 1;
                        touched_slots.push_back(slot);
                    }
                    first_hits_by_slot[slot].push_back(
                        make_fast64_first_hit(point_index, row)
                    );
                    ++stats.bucket_point_adds;
                    return;
                }

                if (!fast64_state_is_promoted(state)) {
                    const uint8_t first_row =
                        static_cast<uint8_t>(state - 1);
                    if (first_row == row) return;

                    // Second hit in a different row: allocate the full mask
                    // now, and only now.
                    std::vector<uint32_t>& promoted_points =
                        touched_points[slot];
                    std::vector<uint64_t>& promoted_masks =
                        promoted_masks_by_slot[slot];
                    const uint32_t promoted_index =
                        static_cast<uint32_t>(promoted_points.size());
                    if (promoted_index >= FAST64_PROMOTED_INDEX_MASK) {
                        throw std::runtime_error(
                            "fast64 promoted-point index overflow"
                        );
                    }
                    promoted_points.push_back(point_index);
                    promoted_masks.push_back(
                        (uint64_t{1} << first_row) | row_bit
                    );
                    state = FAST64_PROMOTED_FLAG | promoted_index;
                    return;
                }

                const uint32_t promoted_index =
                    fast64_promoted_index(state);
                promoted_masks_by_slot[slot][promoted_index] |= row_bit;
            };

            for (PackedWindowEvent ev : events) {
                record_hit(
                    window_event_slot(ev),
                    static_cast<uint32_t>(window_event_point(ev)),
                    window_event_row(ev)
                );
            }
            // 统计scan峰值
            uint64_t first_hits_by_slot_bytes =
                static_cast<uint64_t>(first_hits_by_slot.capacity()) *
                static_cast<uint64_t>(sizeof(std::vector<Fast64FirstHit>));
            for (size_t slot : touched_slots) {
                first_hits_by_slot_bytes +=
                    static_cast<uint64_t>(first_hits_by_slot[slot].capacity()) *
                    static_cast<uint64_t>(sizeof(Fast64FirstHit));
            }
            uint64_t promoted_masks_bytes = 0;
            for (size_t slot : touched_slots) {
                promoted_masks_bytes +=
                    static_cast<uint64_t>(
                        promoted_masks_by_slot[slot].capacity()
                    ) * static_cast<uint64_t>(sizeof(uint64_t));
            }
            stats.memory_fast64_bucket_point_masks_max_bytes = std::max(
                stats.memory_fast64_bucket_point_masks_max_bytes,
                first_hit_state_bytes + promoted_masks_bytes
            );
            stats.memory_fast64_first_hits_max_bytes =
                std::max(stats.memory_fast64_first_hits_max_bytes, first_hits_by_slot_bytes);
            stats.memory_fast64_estimated_peak_tile_max_bytes = std::max(
                stats.memory_fast64_estimated_peak_tile_max_bytes,
                stats.memory_fast64_static_workspace_max_bytes +
                    stats.memory_fast64_nz_scalars_max_bytes +
                    stats.memory_fast64_window_events_max_bytes +
                    first_hits_by_slot_bytes +
                    promoted_masks_bytes
            );
            //scan时间统计
            const auto t_scan1 = std::chrono::steady_clock::now();
            stats.count_scan_seconds += elapsed_seconds(t_scan0, t_scan1);

            /*======================统计mask阶段==========================*/
            const auto t_finalize0 = t_scan1;
            // v32: after scan, only promoted multi-row points are counted here.
            // Singleton first hits are kept out of counts and go to the direct path.
            for (size_t slot : touched_slots) {
                CountMap64& counts = counts_by_slot[slot];
                counts.clear();
                counts.reserve(touched_points[slot].size());
                const std::vector<uint64_t>& promoted_masks =
                    promoted_masks_by_slot[slot];
                for (uint64_t rows : promoted_masks) {
                    // Every entry was created by a second, different-row hit,
                    // so popcount(rows) is guaranteed to be at least two.
                    counts[rows] += 1;
                }
            }
            const auto t_finalize1 = std::chrono::steady_clock::now();
            stats.count_finalize_seconds += elapsed_seconds(t_finalize0, t_finalize1);

            // v18: directly accumulate first-hit entries that never became multi-row,
            // but keep the execution order grouped by slot/bucket. This is the main
            // fix over v17, where global first_hits caused scattered row_buckets writes.
            /*=====================直接把singleton累加=========================*/
            const auto t_singleton0 = std::chrono::steady_clock::now();
            singleton_events.clear();
            for (size_t slot : touched_slots) {
                const int signed_bucket = slot_to_signed_bucket(slot, q);
                for (const Fast64FirstHit& hit : first_hits_by_slot[slot]) {
                    const uint32_t point_index = fast64_first_hit_point(hit);
                    const uint8_t first_row = fast64_first_hit_row(hit);
                    const size_t state_index = slot * n + static_cast<size_t>(point_index);
                    const uint32_t state = first_hit_state[state_index];
                    // A singleton retains first_row + 1. Promoted points carry
                    // FAST64_PROMOTED_FLAG and are handled by the group path.
                    if (state != static_cast<uint32_t>(first_row) + 1) {
                        continue;
                    }

                    ++stats.fallback_point_events;
                    ++stats.singleton_direct_point_events;
                    ++stats.fallback_popcount1_events;
                    singleton_events.push_back(make_singleton_exec_event(
                        point_index,
                        static_cast<size_t>(first_row),
                        q,
                        signed_bucket));
                    stats.fallback_row_adds += 1;
                    stats.singleton_direct_row_adds += 1;
                    stats.raw_row_bucket_adds += 1;
                    stats.optimized_bucket_adds_est += 1;
                    first_hit_state[state_index] = 0;
                }
            }
            execute_singleton_events_adaptive(
                row_buckets,
                points,
                q,
                singleton_events,
                singleton_scatter,
                singleton_target_counts,
                singleton_target_cursors);
            const uint64_t singleton_exec_bytes =
                static_cast<uint64_t>(singleton_events.capacity() +
                                      singleton_scatter.capacity()) *
                    static_cast<uint64_t>(sizeof(SingletonExecEvent)) +
                static_cast<uint64_t>(singleton_target_counts.capacity() +
                                      singleton_target_cursors.capacity()) *
                    static_cast<uint64_t>(sizeof(uint32_t));
            stats.memory_fast64_estimated_peak_tile_max_bytes = std::max(
                stats.memory_fast64_estimated_peak_tile_max_bytes,
                stats.memory_fast64_static_workspace_max_bytes +
                    stats.memory_fast64_window_events_max_bytes +
                    stats.memory_fast64_first_hits_max_bytes +
                    singleton_exec_bytes);
            const auto t_singleton1 = std::chrono::steady_clock::now();
            const double singleton_seconds = elapsed_seconds(t_singleton0, t_singleton1);
            stats.singleton_direct_seconds += singleton_seconds;
            stats.g1_group_build_seconds += singleton_seconds;

            //   for each slot: estimate/select leaves, immediately build groups/direct buckets,
            //   then clear this slot. This keeps leaf.counts hot in cache and avoids storing
            //    all slots' leaf plans until a second pass.
            /*============判断是否进一步拆分tile===============*/
            const auto t_fused_group0 = std::chrono::steady_clock::now();

            for (size_t slot : touched_slots) {
                const int signed_bucket = slot_to_signed_bucket(slot, q);

                if (touched_points[slot].empty() && counts_by_slot[slot].empty()) {
                    slot_touched[slot] = 0;
                    counts_by_slot[slot].clear();
                    continue;
                }

                /*=============估计成本，选择leaf=========*/
                CountMap64& counts = counts_by_slot[slot];
                const uint64_t raw_cost = raw_count_cost64(counts); //初始代价
                stats.raw_row_bucket_adds += raw_cost;

                // const bool enable_multiscale_for_slot =
                //     fast_multiscale_window_selected(opt, w);

                std::vector<Fast64LeafPlan>& leaves = leaf_plans_by_slot[slot];
                leaves.clear();
                // Move the selected count map into the leaf plan instead of copying it.
                // After this call, counts_by_slot[slot] may be empty/moved-from;
                // the group phase must use leaf.counts only.
                select_multiscale_leaves_by_count64(
                    std::move(counts),
                    0,
                    tile_rows,
                    opt,
                    opt.fast_multiscale,
                    w,
                    leaves,
                    stats
                );

                uint64_t opt_cost = 0;
                uint64_t shared_points = 0;
                for (const Fast64LeafPlan& leaf : leaves) {
                    exact_count_cost_stats64(leaf.counts, opt, opt_cost, shared_points);
                }
                stats.optimized_bucket_adds_est += opt_cost;
                stats.exact_mask_saving_est += (raw_cost > opt_cost ? raw_cost - opt_cost : 0);
                stats.shared_group_point_count += shared_points;


                /*====================构造group=====================*/

                // All popcount(rows)==1 events have already been peeled out.
                // The remaining points are multi-row masks only.
                if (leaves.size() == 1) {
                    ++stats.single_leaf_fast_slots;
                } else {
                    ++stats.multi_leaf_fast_slots;
                }
                // Execute each leaf with one fused mask metadata table and a
                // dense G1 group array. No unordered_map is constructed in
                // this hot path.
                for (const Fast64LeafPlan& leaf : leaves) {
                    mask_exec_table.clear();
                    mask_exec_table.reserve(leaf.counts.size());
                    dense_groups.clear();

                    for (const auto& item : leaf.counts) {
                        const uint64_t rows = item.first;
                        const uint32_t count = item.second;
                        MaskExecMeta64 meta;
                        meta.count = count;
                        if (should_share_mask64(count, rows, opt)) {
                            if (dense_groups.size() >= FAST64_NO_GROUP) {
                                throw std::runtime_error(
                                    "fast64 dense group index overflow"
                                );
                            }
                            meta.group_index =
                                static_cast<uint32_t>(dense_groups.size());
                            dense_groups.emplace_back();
                            dense_groups.back().row_mask = rows;
                            ++stats.shareable_mask_entries;
                        }
                        mask_exec_table[rows] = meta;
                    }

                    // Allocate one contiguous point-index range per shareable
                    // group. The following promoted-point pass only classifies;
                    // G1 additions run later in branch-free group ranges.
                    group_offsets.assign(dense_groups.size() + 1, 0);
                    for (const auto& item : leaf.counts) {
                        auto meta_it = mask_exec_table.find(item.first);
                        if (meta_it != mask_exec_table.end() &&
                            meta_it->second.group_index != FAST64_NO_GROUP) {
                            group_offsets[
                                static_cast<size_t>(meta_it->second.group_index) + 1
                            ] = item.second;
                        }
                    }
                    for (size_t g = 1; g < group_offsets.size(); ++g) {
                        group_offsets[g] += group_offsets[g - 1];
                    }
                    group_cursors = group_offsets;
                    if (!group_cursors.empty()) group_cursors.pop_back();
                    const size_t planned_group_points =
                        group_offsets.empty() ? 0 : group_offsets.back();
                    constexpr size_t kContiguousGroupThreshold = 1024;
                    const bool use_contiguous_group_ranges =
                        planned_group_points >= kContiguousGroupThreshold;
                    if (use_contiguous_group_ranges) {
                        group_point_indices.resize(planned_group_points);
                    } else {
                        group_point_indices.clear();
                    }

                    const std::vector<uint32_t>& promoted_points =
                        touched_points[slot];
                    const std::vector<uint64_t>& promoted_masks =
                        promoted_masks_by_slot[slot];
                    for (size_t promoted_index = 0;
                         promoted_index < promoted_points.size();
                         ++promoted_index) {
                        const uint32_t point_index =
                            promoted_points[promoted_index];
                        const uint64_t full_rows =
                            promoted_masks[promoted_index];
                        const uint64_t rows = full_rows & leaf.range_mask;
                        if (rows == 0) continue;
                        ++stats.group_point_visits;

                        if ((rows & (rows - 1)) == 0) {
                            ++stats.fallback_point_events;
                            stats.fallback_row_adds += add_point_to_row_buckets64_fallback_fast(
                                row_buckets,
                                q,
                                signed_bucket,
                                rows,
                                points[point_index],
                                stats
                            );
                            continue;
                        }

                        auto meta_it = mask_exec_table.find(rows);
                        if (meta_it != mask_exec_table.end() &&
                            meta_it->second.group_index != FAST64_NO_GROUP) {
                            ++stats.shareable_mask_hits;
                            const uint32_t group_index =
                                meta_it->second.group_index;
                            if (use_contiguous_group_ranges) {
                                group_point_indices[
                                    group_cursors[group_index]++
                                ] = point_index;
                            } else {
                                MaskPointGroup& group =
                                    dense_groups[group_index];
                                if (group.count == 0) {
                                    group.sum = points[point_index];
                                } else {
                                    group.sum += points[point_index];
                                }
                                ++group.count;
                            }
                            ++stats.share_point_events;
                        } else {
                            ++stats.nonshareable_mask_misses;
                            ++stats.fallback_point_events;
                            stats.fallback_row_adds += add_point_to_row_buckets64_fallback_fast(
                                row_buckets,
                                q,
                                signed_bucket,
                                rows,
                                points[point_index],
                                stats
                            );
                        }
                    }

                    for (size_t group_index = 0;
                         group_index < dense_groups.size();
                         ++group_index) {
                        const size_t begin = group_offsets[group_index];
                        const size_t end = group_offsets[group_index + 1];
                        if (begin == end) continue;
                        MaskPointGroup& group = dense_groups[group_index];
                        if (use_contiguous_group_ranges) {
                            group.sum = points[group_point_indices[begin]];
                            for (size_t pos = begin + 1; pos < end; ++pos) {
                                group.sum += points[group_point_indices[pos]];
                            }
                            group.count = static_cast<uint32_t>(end - begin);
                        }
                        add_group_to_row_buckets64(
                            row_buckets,
                            q,
                            signed_bucket,
                            group.row_mask,
                            group.sum
                        );
                        ++stats.shared_bucket_groups;
                        const uint64_t row_links =
                            static_cast<uint64_t>(popcount64(group.row_mask));
                        stats.shared_bucket_row_links += row_links;
                        stats.shared_link_row_adds += row_links;
                    }
                    const uint64_t group_exec_bytes =
                        static_cast<uint64_t>(group_offsets.capacity() +
                                              group_cursors.capacity() +
                                              group_point_indices.capacity()) *
                        static_cast<uint64_t>(sizeof(uint32_t));
                    stats.memory_fast64_estimated_peak_tile_max_bytes = std::max(
                        stats.memory_fast64_estimated_peak_tile_max_bytes,
                        stats.memory_fast64_static_workspace_max_bytes +
                            stats.memory_fast64_window_events_max_bytes +
                            stats.memory_fast64_first_hits_max_bytes +
                            group_exec_bytes);
                }

                // Clear this slot immediately. This shortens the lifetime of per-slot data
                // and improves cache locality versus clearing all slots after a second pass.
                for (uint32_t point_index : touched_points[slot]) {
                    const size_t state_index = slot * n + static_cast<size_t>(point_index);
                    first_hit_state[state_index] = 0;
                }
                touched_points[slot].clear();
                promoted_masks_by_slot[slot].clear();
                slot_touched[slot] = 0;
                counts_by_slot[slot].clear();
                leaves.clear();
            }
            for (size_t slot : touched_slots) {
                first_hits_by_slot[slot].clear();
                // Defensive cleanup for singleton-only slots. Singleton
                // entries were already reset in the direct loop; promoted
                // entries were reset above.
                promoted_masks_by_slot[slot].clear();
            }
            touched_slots.clear();

            const auto t_fused_group1 = std::chrono::steady_clock::now();
            const double fused_group_total_seconds = elapsed_seconds(t_fused_group0, t_fused_group1);
            stats.count_build_seconds += elapsed_seconds(t_count0, t_finalize1);
            stats.group_event_loop_seconds += fused_group_total_seconds;
            stats.g1_group_build_seconds += fused_group_total_seconds;
        }

        const auto t_running0 = std::chrono::steady_clock::now();
        for (size_t r = 0; r < tile_rows; ++r) {
            G1 running;
            running.clear();
            for (uint32_t d = q - 1; d >= 1; --d) {
                running += row_buckets[r * q + d];
                row_outputs[r] += running;
                if (d == 1) break;
            }
        }
        const auto t_running1 = std::chrono::steady_clock::now();
        stats.running_sum_seconds += elapsed_seconds(t_running0, t_running1);
    }
    return stats;
}

TileMaskStats tile_direct_pippenger_rows(
    std::vector<G1>& row_outputs,
    const std::vector<G1>& points,
    const std::vector<uint64_t>& scalar_tile,
    size_t tile_rows,
    size_t n,
    int c,
    const Options& opt,
    TileDirectPippengerThreadWorkspace* reusable_workspace = nullptr
) {
    if (tile_rows > 64) {
        throw std::runtime_error("tile_direct_pippenger_rows requires tile_rows <= 64");
    }

    const uint32_t q = uint32_t{1} << c;
    const uint32_t digit_mask = q - 1;
    const int windows = (opt.scalar_bits + c - 1) / c;
    const bool use_signed128_nz =
        (opt.scalar_format_kind == ScalarFormatKind::Signed128);
    TileMaskStats stats;

    row_outputs.assign(tile_rows, G1());
    for (size_t r = 0; r < tile_rows; ++r) row_outputs[r].clear();

    TileDirectPippengerThreadWorkspace local_workspace;
    TileDirectPippengerThreadWorkspace& ws =
        reusable_workspace ? *reusable_workspace : local_workspace;
    std::vector<NzScalar128>& nz_scalars128 = ws.nz_scalars128;
    std::vector<NzScalar256>& nz_scalars256 = ws.nz_scalars256;

    // 三种格式都在 tile 级别先过滤“整个 scalar 为零”的条目。
    // signed128 使用两个 limb 的紧凑结构；bn254 / bn254-balanced
    // 保留完整四 limb，并在 balanced 模式下预先转换成绝对值和符号。
    const auto t_predecode0 = std::chrono::steady_clock::now();
    uint64_t zero_count = 0;
    uint64_t nonzero_count = 0;
    uint64_t predecode_capacity = 0;

    if (use_signed128_nz) {
        nz_scalars256.clear();
        build_nonzero_signed128_tile(
            scalar_tile,
            tile_rows,
            n,
            nz_scalars128,
            zero_count
        );
        nonzero_count = static_cast<uint64_t>(nz_scalars128.size());
        predecode_capacity = static_cast<uint64_t>(nz_scalars128.capacity());
    } else {
        nz_scalars128.clear();
        build_nonzero_bn254_tile(
            scalar_tile,
            tile_rows,
            n,
            opt.scalar_format_kind,
            nz_scalars256,
            zero_count
        );
        nonzero_count = static_cast<uint64_t>(nz_scalars256.size());
        predecode_capacity = static_cast<uint64_t>(nz_scalars256.capacity());
    }

    const auto t_predecode1 = std::chrono::steady_clock::now();
    const double predecode_seconds =
        elapsed_seconds(t_predecode0, t_predecode1);

    stats.scalar_predecode_seconds += predecode_seconds;
    stats.count_scan_seconds += predecode_seconds;
    stats.count_build_seconds += predecode_seconds;
    ++stats.scalar_predecode_tiles;
    stats.scalar_predecode_nonzero_scalars += nonzero_count;
    stats.scalar_predecode_zero_scalars += zero_count;
    stats.scalar_predecode_capacity += predecode_capacity;

    if (nonzero_count == 0) {
        ++stats.zero_tile_skipped_count;
        return stats;
    }

    const auto t_workspace_init0 = std::chrono::steady_clock::now();
    ws.prepare(tile_rows, q);
    const auto t_workspace_init1 = std::chrono::steady_clock::now();
    stats.fast64_workspace_init_seconds +=
        elapsed_seconds(t_workspace_init0, t_workspace_init1);

    std::vector<G1>& row_buckets = ws.row_buckets;
    const uint64_t row_buckets_bytes =
        static_cast<uint64_t>(row_buckets.capacity()) *
        static_cast<uint64_t>(sizeof(G1));
    const uint64_t nz_scalars_bytes = use_signed128_nz
        ? static_cast<uint64_t>(nz_scalars128.capacity()) *
              static_cast<uint64_t>(sizeof(NzScalar128))
        : static_cast<uint64_t>(nz_scalars256.capacity()) *
              static_cast<uint64_t>(sizeof(NzScalar256));
    stats.memory_fast64_row_buckets_max_bytes = row_buckets_bytes;
    stats.memory_fast64_nz_scalars_max_bytes = nz_scalars_bytes;
    stats.memory_fast64_static_workspace_max_bytes = row_buckets_bytes;
    stats.memory_fast64_estimated_peak_tile_max_bytes =
        row_buckets_bytes + nz_scalars_bytes;

    bool have_window = false;
    for (int w = windows - 1; w >= 0; --w) {
        if (have_window) {
            for (int bit = 0; bit < c; ++bit) {
                for (size_t r = 0; r < tile_rows; ++r) {
                    row_outputs[r] += row_outputs[r];
                }
            }
        }
        have_window = true;

        const auto t_workspace_clear0 = std::chrono::steady_clock::now();
        for (G1& bucket : row_buckets) bucket.clear();
        const auto t_workspace_clear1 = std::chrono::steady_clock::now();
        stats.tile_workspace_clear_seconds +=
            elapsed_seconds(t_workspace_clear0, t_workspace_clear1);

        const int shift = w * c;
        const auto t_direct0 = std::chrono::steady_clock::now();
        uint64_t direct_adds = 0;

        if (use_signed128_nz) {
            for (const NzScalar128& ev : nz_scalars128) {
                const uint32_t d = extract_digit(
                    ev.abs_lo,
                    ev.abs_hi,
                    shift,
                    digit_mask
                );
                if (d == 0) continue;

                const int signed_bucket =
                    ev.neg ? -static_cast<int>(d) : static_cast<int>(d);
                add_point_to_single_row_bucket64(
                    row_buckets,
                    q,
                    signed_bucket,
                    static_cast<size_t>(ev.row),
                    points[ev.point_index]
                );
                ++direct_adds;
            }
        } else {
            for (const NzScalar256& ev : nz_scalars256) {
                const uint32_t d = extract_digit_u256(
                    ev.magnitude,
                    shift,
                    digit_mask
                );
                if (d == 0) continue;

                const int signed_bucket =
                    ev.neg ? -static_cast<int>(d) : static_cast<int>(d);
                add_point_to_single_row_bucket64(
                    row_buckets,
                    q,
                    signed_bucket,
                    static_cast<size_t>(ev.row),
                    points[ev.point_index]
                );
                ++direct_adds;
            }
        }

        const auto t_direct1 = std::chrono::steady_clock::now();
        const double direct_seconds = elapsed_seconds(t_direct0, t_direct1);
        stats.direct_bucket_accum_seconds += direct_seconds;
        stats.direct_window_seconds += direct_seconds;
        stats.g1_group_build_seconds += direct_seconds;
        stats.raw_row_bucket_adds += direct_adds;
        stats.optimized_bucket_adds_est += direct_adds;
        stats.fast_direct_row_bucket_adds += direct_adds;

        const auto t_running0 = std::chrono::steady_clock::now();
        for (size_t r = 0; r < tile_rows; ++r) {
            G1 running;
            running.clear();
            for (uint32_t d = q - 1; d >= 1; --d) {
                running += row_buckets[r * q + d];
                row_outputs[r] += running;
                if (d == 1) break;
            }
        }
        const auto t_running1 = std::chrono::steady_clock::now();
        stats.running_sum_seconds += elapsed_seconds(t_running0, t_running1);
    }

    return stats;
}

std::vector<G1> load_points(const std::string& path, size_t n, size_t point_offset) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open points file: " + path);
    }
    NpyHeader h = read_npy_header(in, path);
    if (h.shape.size() != 2 || h.shape[1] != 4) {
        throw std::runtime_error("Points file must have shape (point_count, 4)");
    }
    if (point_offset > h.shape[0] || n > h.shape[0] - point_offset) {
        throw std::runtime_error("Not enough points in points file for n and point_offset");
    }

    std::vector<G1> points(n);
    in.seekg(h.data_offset + static_cast<std::streamoff>(point_offset * 4 * sizeof(uint64_t)));
    std::vector<uint64_t> row(4);
    unsigned char buf[32];
    for (size_t i = 0; i < n; ++i) {
        in.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size() * sizeof(uint64_t)));
        if (!in) {
            throw std::runtime_error("Failed to read point row");
        }
        std::memcpy(buf, row.data(), sizeof(buf));
        const size_t nread = points[i].deserialize(buf, sizeof(buf), mcl::IoSerialize);
        if (nread != sizeof(buf)) {
            std::ostringstream oss;
            oss << "G1 deserialize failed at point index " << (point_offset + i)
                << ", bytes_read=" << nread
                << ", uint64_limbs=" << limbs_to_string(row)
                << ", bytes_hex=" << bytes_to_hex(buf, sizeof(buf));
            throw std::runtime_error(oss.str());
        }
    }
    return points;
}

std::vector<G1> generate_random_points(size_t n, size_t point_offset, const std::string& seed) {
    const G1 base = bn254_explicit_base_point();
    std::vector<G1> points(n);
    for (size_t i = 0; i < n; ++i) {
        Fr s = scalar_for_point_index(point_offset + i, seed);
        points[i] = base * s;
    }
    return points;
}

std::vector<G1> prepare_points(const Options& opt) {
    if (opt.points_mode == "file") {
        return load_points(opt.points_path, opt.n, opt.point_offset);
    }
    return generate_random_points(opt.n, opt.point_offset, opt.point_seed);
}

void write_stats(const Options& opt, const TimingStats& timing) {
    if (opt.stats_path.empty()) return;

    std::ofstream out(opt.stats_path);
    if (!out) {
        throw std::runtime_error("Failed to open stats output: " + opt.stats_path);
    }

    const std::string pippenger_algorithm =
        (opt.scalar_format_kind == ScalarFormatKind::Signed128)
            ? "tile_direct_pippenger_signed128_sparse"
            : (opt.scalar_format_kind == ScalarFormatKind::Bn254Balanced)
                ? "tile_direct_pippenger_bn254_balanced_sparse"
                : "tile_direct_pippenger_bn254_sparse";
    const std::string algorithm =
        (opt.mode == "pippenger")
            ? pippenger_algorithm
            : (opt.fast_multiscale
                ? "tile_mask_fast64_v39_fused_adaptive_multiscale"
                : "tile_mask_fast64_v39_fused_adaptive_exact");

    const double saving_ratio = timing.raw_row_bucket_adds > 0
        ? static_cast<double>(timing.exact_mask_saving_est) /
              static_cast<double>(timing.raw_row_bucket_adds)
        : 0.0;

    const uint64_t scalar_total =
        timing.scalar_predecode_nonzero_scalars +
        timing.scalar_predecode_zero_scalars;
    const double scalar_nonzero_ratio = scalar_total > 0
        ? static_cast<double>(timing.scalar_predecode_nonzero_scalars) /
              static_cast<double>(scalar_total)
        : 0.0;

    const double attributed_compute = (opt.mode == "pippenger")
        ? timing.scalar_predecode_seconds +
              timing.direct_bucket_accum_seconds +
              timing.tile_running_sum_seconds
        : timing.tile_g1_group_build_seconds +
              timing.scalar_predecode_seconds +
              timing.event_precompute_seconds +
              timing.tile_count_build_seconds +
              timing.tile_running_sum_seconds;
    const double other_compute = timing.compute_seconds > attributed_compute
        ? timing.compute_seconds - attributed_compute
        : 0.0;

    out << "{\n";
    out << "  \"algorithm\": " << std::quoted(algorithm) << ",\n";
    out << "  \"curve\": \"BN254\",\n";
    out << "  \"scalars_path\": " << std::quoted(opt.scalars_path) << ",\n";
    out << "  \"points_path\": " << std::quoted(opt.points_path) << ",\n";
    out << "  \"points_mode\": " << std::quoted(opt.points_mode) << ",\n";
    out << "  \"point_seed\": " << std::quoted(opt.point_seed) << ",\n";
    out << "  \"output_path\": " << std::quoted(opt.out_path) << ",\n";
    out << "  \"mode\": " << std::quoted(opt.mode) << ",\n";
    out << "  \"K\": " << opt.K << ",\n";
    out << "  \"n\": " << opt.n << ",\n";
    out << "  \"row_start\": " << opt.row_start << ",\n";
    out << "  \"row_count\": " << opt.row_count << ",\n";
    out << "  \"point_offset\": " << opt.point_offset << ",\n";
    out << "  \"c\": " << opt.c << ",\n";
    out << "  \"scalar_bits\": " << opt.scalar_bits << ",\n";
    out << "  \"scalar_format\": " << std::quoted(opt.scalar_format) << ",\n";
    out << "  \"threads\": " << opt.threads << ",\n";
    out << "  \"tile_size\": " << opt.tile_size << ",\n";

    out << "  \"fast64_options\": {\n";
    out << "    \"min_gain\": " << opt.fast_min_gain << ",\n";
    out << "    \"min_group_count\": " << opt.fast_min_group_count << ",\n";
    out << "    \"fast_windows\": " << std::quoted(opt.fast_windows_spec) << ",\n";
    out << "    \"multiscale_tile\": " << (opt.fast_multiscale ? "true" : "false") << ",\n";
    out << "    \"multiscale_decision\": " << std::quoted(opt.fast_multiscale_decision) << ",\n";
    out << "    \"base_tile_size\": " << opt.split_base_tile_size << ",\n";
    out << "    \"split_factor\": " << opt.split_factor << "\n";
    out << "  },\n";

    out << "  \"shared_bucket_stats\": {\n";
    out << "    \"bucket_point_adds\": " << timing.bucket_point_adds << ",\n";
    out << "    \"raw_row_bucket_adds\": " << timing.raw_row_bucket_adds << ",\n";
    out << "    \"optimized_bucket_adds_est\": " << timing.optimized_bucket_adds_est << ",\n";
    out << "    \"exact_mask_saving_est\": " << timing.exact_mask_saving_est << ",\n";
    out << "    \"exact_mask_saving_ratio_est\": " << std::setprecision(12) << saving_ratio << ",\n";
    out << "    \"shared_group_point_count\": " << timing.shared_group_point_count << ",\n";
    out << "    \"fast_direct_row_bucket_adds\": " << timing.fast_direct_row_bucket_adds << ",\n";
    out << "    \"shared_bucket_groups\": " << timing.shared_bucket_groups << ",\n";
    out << "    \"shared_bucket_row_links\": " << timing.shared_bucket_row_links << ",\n";
    out << "    \"group_point_visits\": " << timing.group_point_visits << ",\n";
    out << "    \"share_point_events\": " << timing.share_point_events << ",\n";
    out << "    \"fallback_point_events\": " << timing.fallback_point_events << ",\n";
    out << "    \"fallback_row_adds\": " << timing.fallback_row_adds << ",\n";
    out << "    \"fallback_popcount1_events\": " << timing.fallback_popcount1_events << ",\n";
    out << "    \"shared_link_row_adds\": " << timing.shared_link_row_adds << ",\n";
    out << "    \"single_leaf_fast_slots\": " << timing.single_leaf_fast_slots << ",\n";
    out << "    \"multi_leaf_fast_slots\": " << timing.multi_leaf_fast_slots << ",\n";
    out << "    \"shareable_mask_entries\": " << timing.shareable_mask_entries << ",\n";
    out << "    \"shareable_mask_hits\": " << timing.shareable_mask_hits << ",\n";
    out << "    \"nonshareable_mask_misses\": " << timing.nonshareable_mask_misses << ",\n";
    out << "    \"singleton_direct_point_events\": " << timing.singleton_direct_point_events << ",\n";
    out << "    \"singleton_direct_row_adds\": " << timing.singleton_direct_row_adds << ",\n";
    out << "    \"multiscale_parent_chosen\": " << timing.multiscale_parent_chosen << ",\n";
    out << "    \"multiscale_child_chosen\": " << timing.multiscale_child_chosen << ",\n";
    out << "    \"zero_tile_skipped_count\": " << timing.zero_tile_skipped_count << "\n";
    out << "  },\n";

    out << "  \"scalar_predecode_stats\": {\n";
    out << "    \"tiles\": " << timing.scalar_predecode_tiles << ",\n";
    out << "    \"nonzero_scalars\": " << timing.scalar_predecode_nonzero_scalars << ",\n";
    out << "    \"zero_scalars\": " << timing.scalar_predecode_zero_scalars << ",\n";
    out << "    \"total_scalars\": " << scalar_total << ",\n";
    out << "    \"nonzero_ratio\": " << std::setprecision(12) << scalar_nonzero_ratio << ",\n";
    out << "    \"capacity_entries_sum\": " << timing.scalar_predecode_capacity << ",\n";
    out << "    \"sizeof_NzScalar128\": " << sizeof(NzScalar128) << ",\n";
    out << "    \"sizeof_NzScalar256\": " << sizeof(NzScalar256) << "\n";
    out << "  },\n";

    out << "  \"memory_stats\": {\n";
    out << "    \"sizeof_G1\": " << sizeof(G1) << ",\n";
    out << "    \"row_buckets_max_bytes\": " << timing.memory_fast64_row_buckets_max_bytes << ",\n";
    out << "    \"bucket_point_masks_max_bytes\": " << timing.memory_fast64_bucket_point_masks_max_bytes << ",\n";
        out << "    \"bucket_point_masks_storage\": "
            << std::quoted("fused_decode_events_plus_first_hit_promoted_masks_and_adaptive_exec")
            << ",\n";
    out << "    \"nz_scalars_max_bytes\": " << timing.memory_fast64_nz_scalars_max_bytes << ",\n";
    out << "    \"window_events_max_bytes\": " << timing.memory_fast64_window_events_max_bytes << ",\n";
    out << "    \"first_hits_by_slot_max_bytes\": " << timing.memory_fast64_first_hits_max_bytes << ",\n";
    out << "    \"sizeof_first_hit_entry\": " << sizeof(Fast64FirstHit) << ",\n";
    out << "    \"static_workspace_max_bytes\": " << timing.memory_fast64_static_workspace_max_bytes << ",\n";
    out << "    \"estimated_peak_tile_workspace_max_bytes\": " << timing.memory_fast64_estimated_peak_tile_max_bytes << "\n";
    out << "  },\n";

    auto phase_ratio = [&](double seconds) {
        return timing.compute_seconds > 0.0
            ? seconds / timing.compute_seconds
            : 0.0;
    };
    out << "  \"timing\": {\n";
    out << "    \"init_seconds\": " << std::setprecision(12) << timing.init_seconds << ",\n";
    out << "    \"load_points_seconds\": " << timing.load_points_seconds << ",\n";
    out << "    \"scalar_header_seconds\": " << timing.scalar_header_seconds << ",\n";
    out << "    \"tile_scalar_read_seconds\": " << timing.tile_scalar_read_seconds << ",\n";
    out << "    \"compute_seconds\": " << timing.compute_seconds << ",\n";
    out << "    \"row_pippenger_seconds\": 0,\n";
    out << "    \"tile_direct_sparse_seconds\": " << timing.tile_direct_sparse_seconds << ",\n";
    out << "    \"signed128_predecode_seconds\": " << timing.scalar_predecode_seconds << ",\n";
    out << "    \"event_precompute_seconds\": " << timing.event_precompute_seconds << ",\n";
    out << "    \"direct_bucket_accum_seconds\": " << timing.direct_bucket_accum_seconds << ",\n";
    out << "    \"tile_running_sum_seconds\": " << timing.tile_running_sum_seconds << ",\n";
    out << "    \"compute_seconds_note\": "
        << std::quoted(
               "compute_seconds excludes tile_scalar_read_seconds; ratios below use "
               "compute_seconds as denominator; phase seconds are sums across worker threads")
        << ",\n";
    out << "    \"phase_timing\": {\n";
    out << "      \"tile_g1_group_build\": {\"seconds\": "
        << timing.tile_g1_group_build_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_g1_group_build_seconds) << "},\n";
    out << "      \"tile_count_build\": {\"seconds\": "
        << timing.tile_count_build_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_count_build_seconds) << "},\n";
    out << "      \"scalar_predecode\": {\"seconds\": "
        << timing.scalar_predecode_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.scalar_predecode_seconds) << "},\n";
    out << "      \"event_precompute\": {\"seconds\": "
        << timing.event_precompute_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.event_precompute_seconds) << "},\n";
    out << "      \"tile_running_sum\": {\"seconds\": "
        << timing.tile_running_sum_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_running_sum_seconds) << "},\n";
    out << "      \"other_compute\": {\"seconds\": " << other_compute
        << ", \"compute_ratio\": " << phase_ratio(other_compute) << "}\n";
    out << "    },\n";
    out << "    \"phase_timing_detail\": {\n";
    out << "      \"singleton_finalize_direct_accum\": {\"seconds\": "
        << timing.tile_singleton_direct_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_singleton_direct_seconds) << "},\n";
    out << "      \"group_event_loop_by_bucket_slot\": {\"seconds\": "
        << timing.tile_group_event_loop_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_group_event_loop_seconds) << "},\n";
    out << "      \"direct_disabled_window_accum\": {\"seconds\": "
        << timing.tile_direct_window_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_direct_window_seconds) << "},\n";
    out << "      \"count_scan_digit_decode_mask_fill\": {\"seconds\": "
        << timing.tile_count_scan_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_count_scan_seconds) << "},\n";
    out << "      \"finalize_masks_to_counts\": {\"seconds\": "
        << timing.tile_count_finalize_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.tile_count_finalize_seconds) << "},\n";
    out << "      \"signed128_predecode_visible_part\": {\"seconds\": "
        << timing.scalar_predecode_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.scalar_predecode_seconds) << "},\n";
    out << "      \"packed_window_event_precompute\": {\"seconds\": "
        << timing.event_precompute_seconds
        << ", \"compute_ratio\": "
        << phase_ratio(timing.event_precompute_seconds) << "},\n";
    out << "      \"other_compute\": {\"seconds\": " << other_compute
        << ", \"compute_ratio\": " << phase_ratio(other_compute) << "}\n";
    out << "    },\n";
    out << "    \"write_output_seconds\": " << timing.write_output_seconds << ",\n";
    out << "    \"total_seconds\": " << timing.total_seconds << "\n";
    out << "  },\n";
    out << "  \"seconds\": " << timing.total_seconds << "\n";
    out << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto t_total0 = std::chrono::steady_clock::now();
        Options opt = parse_args(argc, argv);
        TimingStats timing;

        const auto t_init0 = std::chrono::steady_clock::now();
        mcl::bn::initPairing(mcl::BN254);
        const auto t_init1 = std::chrono::steady_clock::now();
        timing.init_seconds = elapsed_seconds(t_init0, t_init1);

        if (opt.points_mode == "file") {
            std::cerr << "[bn254-msm] loading " << opt.n << " points from "
                      << opt.points_path << " (offset=" << opt.point_offset << ")\n";
        } else {
            std::cerr << "[bn254-msm] generating " << opt.n
                      << " random BN254 points (offset=" << opt.point_offset;
            std::cerr << (opt.point_seed.empty() ? ", csprng" : ", seed=" + opt.point_seed);
            std::cerr << ")\n";
        }

        const auto t_points0 = std::chrono::steady_clock::now();
        const std::vector<G1> points = prepare_points(opt);
        const auto t_points1 = std::chrono::steady_clock::now();
        timing.load_points_seconds = elapsed_seconds(t_points0, t_points1);

        const auto t_scalar_header0 = std::chrono::steady_clock::now();
        std::ifstream scalar_probe(opt.scalars_path, std::ios::binary);
        if (!scalar_probe) {
            throw std::runtime_error("Failed to open scalar file: " + opt.scalars_path);
        }
        const NpyHeader scalar_header = read_npy_header(scalar_probe, opt.scalars_path);
        validate_scalar_shape(scalar_header, opt.K, opt.n);
        scalar_probe.close();
        const auto t_scalar_header1 = std::chrono::steady_clock::now();
        timing.scalar_header_seconds = elapsed_seconds(t_scalar_header0, t_scalar_header1);

        std::vector<uint64_t> output(opt.row_count * 4, 0);
        const size_t tile_count =
            (opt.row_count + opt.tile_size - 1) / opt.tile_size;

        std::atomic<size_t> next_tile{0};
        std::atomic<size_t> done_rows{0};

        std::atomic<uint64_t> bucket_point_adds{0};
        std::atomic<uint64_t> raw_row_bucket_adds{0};
        std::atomic<uint64_t> optimized_bucket_adds_est{0};
        std::atomic<uint64_t> exact_mask_saving_est{0};
        std::atomic<uint64_t> shared_group_point_count{0};
        std::atomic<uint64_t> fast_direct_row_bucket_adds{0};
        std::atomic<uint64_t> shared_bucket_groups{0};
        std::atomic<uint64_t> shared_bucket_row_links{0};
        std::atomic<uint64_t> group_point_visits{0};
        std::atomic<uint64_t> share_point_events{0};
        std::atomic<uint64_t> fallback_point_events{0};
        std::atomic<uint64_t> fallback_row_adds{0};
        std::atomic<uint64_t> fallback_popcount1_events{0};
        std::atomic<uint64_t> shared_link_row_adds{0};
        std::atomic<uint64_t> single_leaf_fast_slots{0};
        std::atomic<uint64_t> multi_leaf_fast_slots{0};
        std::atomic<uint64_t> shareable_mask_entries{0};
        std::atomic<uint64_t> shareable_mask_hits{0};
        std::atomic<uint64_t> nonshareable_mask_misses{0};
        std::atomic<uint64_t> singleton_direct_point_events{0};
        std::atomic<uint64_t> singleton_direct_row_adds{0};
        std::atomic<uint64_t> multiscale_parent_chosen{0};
        std::atomic<uint64_t> multiscale_child_chosen{0};
        std::atomic<uint64_t> zero_tile_skipped_count{0};

        std::atomic<uint64_t> scalar_predecode_tiles{0};
        std::atomic<uint64_t> scalar_predecode_nonzero_scalars{0};
        std::atomic<uint64_t> scalar_predecode_zero_scalars{0};
        std::atomic<uint64_t> scalar_predecode_capacity{0};

        std::atomic<uint64_t> memory_row_buckets_max{0};
        std::atomic<uint64_t> memory_bucket_masks_max{0};
        std::atomic<uint64_t> memory_nz_scalars_max{0};
        std::atomic<uint64_t> memory_first_hits_max{0};
        std::atomic<uint64_t> memory_static_workspace_max{0};
        std::atomic<uint64_t> memory_peak_tile_max{0};
        std::atomic<uint64_t> memory_window_events_max{0};

        std::atomic<uint64_t> count_build_us{0};
        std::atomic<uint64_t> count_scan_us{0};
        std::atomic<uint64_t> count_finalize_us{0};
        std::atomic<uint64_t> direct_window_us{0};
        std::atomic<uint64_t> g1_group_build_us{0};
        std::atomic<uint64_t> group_event_loop_us{0};
        std::atomic<uint64_t> singleton_direct_us{0};
        std::atomic<uint64_t> running_sum_us{0};
        std::atomic<uint64_t> scalar_predecode_us{0};
        std::atomic<uint64_t> event_precompute_us{0};
        std::atomic<uint64_t> direct_bucket_accum_us{0};
        std::atomic<uint64_t> workspace_clear_us{0};
        std::atomic<uint64_t> workspace_init_us{0};
        std::atomic<uint64_t> scalar_alloc_us{0};
        std::atomic<uint64_t> scalar_read_us{0};
        std::atomic<uint64_t> serialize_us{0};
        std::atomic<uint64_t> atomic_merge_us{0};
        std::atomic<uint64_t> dispatch_wait_us{0};

        std::mutex log_mutex;
        std::mutex error_mutex;
        std::string first_error;

        std::vector<ThreadWallTimeRecord> thread_wall_times(opt.threads);
        for (unsigned t = 0; t < opt.threads; ++t) {
            thread_wall_times[t].thread_id = t;
        }

        const auto t_compute0 = std::chrono::steady_clock::now();
        auto tile_worker = [&](unsigned tid) {
            const auto t_worker0 = std::chrono::steady_clock::now();
            double local_count_build = 0.0;
            double local_g1_group_build = 0.0;
            double local_running_sum = 0.0;
            double local_scalar_alloc = 0.0;
            double local_scalar_read = 0.0;
            double local_workspace_init = 0.0;
            double local_serialize = 0.0;
            double local_atomic_merge = 0.0;
            double local_dispatch_wait = 0.0;

            try {
                std::ifstream scalars(opt.scalars_path, std::ios::binary);
                if (!scalars) {
                    throw std::runtime_error("Worker failed to open scalar file");
                }

                std::vector<uint64_t> tile_scalars;
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    tile_scalars.resize(opt.tile_size * opt.n * 4);
                    const auto t1 = std::chrono::steady_clock::now();
                    local_scalar_alloc += elapsed_seconds(t0, t1);
                }

                Fast64ThreadWorkspace fast64_workspace;
                TileDirectPippengerThreadWorkspace direct_workspace;

                while (true) {
                    const auto td0 = std::chrono::steady_clock::now();
                    const size_t tile_id = next_tile.fetch_add(1);
                    const auto td1 = std::chrono::steady_clock::now();
                    local_dispatch_wait += elapsed_seconds(td0, td1);
                    if (tile_id >= tile_count) break;

                    const size_t local_start = tile_id * opt.tile_size;
                    const size_t tile_rows =
                        std::min(opt.tile_size, opt.row_count - local_start);
                    const size_t global_start = opt.row_start + local_start;
                    const size_t entries = tile_rows * opt.n * 4;

                    if (tile_scalars.size() < entries) {
                        const auto t0 = std::chrono::steady_clock::now();
                        tile_scalars.resize(entries);
                        const auto t1 = std::chrono::steady_clock::now();
                        local_scalar_alloc += elapsed_seconds(t0, t1);
                    }

                    const auto tr0 = std::chrono::steady_clock::now();
                    scalars.clear();
                    scalars.seekg(scalar_row_offset(scalar_header, global_start, opt.n));
                    scalars.read(
                        reinterpret_cast<char*>(tile_scalars.data()),
                        static_cast<std::streamsize>(entries * sizeof(uint64_t))
                    );
                    if (!scalars) {
                        std::ostringstream oss;
                        oss << "Failed to read scalar tile " << tile_id
                            << ", global_start=" << global_start
                            << ", tile_rows=" << tile_rows;
                        throw std::runtime_error(oss.str());
                    }
                    const auto tr1 = std::chrono::steady_clock::now();
                    local_scalar_read += elapsed_seconds(tr0, tr1);

                    std::vector<G1> tile_outputs;
                    TileMaskStats local_stats;
                    if (opt.mode == "pippenger") {
                        local_stats = tile_direct_pippenger_rows(
                            tile_outputs,
                            points,
                            tile_scalars,
                            tile_rows,
                            opt.n,
                            opt.c,
                            opt,
                            &direct_workspace
                        );
                    } else {
                        local_stats = pippenger_tile_mask_fast64_rows(
                            tile_outputs,
                            points,
                            tile_scalars,
                            tile_rows,
                            opt.n,
                            opt.c,
                            opt,
                            &fast64_workspace
                        );
                    }

                    const auto tm0 = std::chrono::steady_clock::now();
                    bucket_point_adds.fetch_add(local_stats.bucket_point_adds);
                    raw_row_bucket_adds.fetch_add(local_stats.raw_row_bucket_adds);
                    optimized_bucket_adds_est.fetch_add(local_stats.optimized_bucket_adds_est);
                    exact_mask_saving_est.fetch_add(local_stats.exact_mask_saving_est);
                    shared_group_point_count.fetch_add(local_stats.shared_group_point_count);
                    fast_direct_row_bucket_adds.fetch_add(local_stats.fast_direct_row_bucket_adds);
                    shared_bucket_groups.fetch_add(local_stats.shared_bucket_groups);
                    shared_bucket_row_links.fetch_add(local_stats.shared_bucket_row_links);
                    group_point_visits.fetch_add(local_stats.group_point_visits);
                    share_point_events.fetch_add(local_stats.share_point_events);
                    fallback_point_events.fetch_add(local_stats.fallback_point_events);
                    fallback_row_adds.fetch_add(local_stats.fallback_row_adds);
                    fallback_popcount1_events.fetch_add(local_stats.fallback_popcount1_events);
                    shared_link_row_adds.fetch_add(local_stats.shared_link_row_adds);
                    single_leaf_fast_slots.fetch_add(local_stats.single_leaf_fast_slots);
                    multi_leaf_fast_slots.fetch_add(local_stats.multi_leaf_fast_slots);
                    shareable_mask_entries.fetch_add(local_stats.shareable_mask_entries);
                    shareable_mask_hits.fetch_add(local_stats.shareable_mask_hits);
                    nonshareable_mask_misses.fetch_add(local_stats.nonshareable_mask_misses);
                    singleton_direct_point_events.fetch_add(local_stats.singleton_direct_point_events);
                    singleton_direct_row_adds.fetch_add(local_stats.singleton_direct_row_adds);
                    multiscale_parent_chosen.fetch_add(local_stats.multiscale_parent_chosen);
                    multiscale_child_chosen.fetch_add(local_stats.multiscale_child_chosen);
                    zero_tile_skipped_count.fetch_add(local_stats.zero_tile_skipped_count);

                    scalar_predecode_tiles.fetch_add(local_stats.scalar_predecode_tiles);
                    scalar_predecode_nonzero_scalars.fetch_add(local_stats.scalar_predecode_nonzero_scalars);
                    scalar_predecode_zero_scalars.fetch_add(local_stats.scalar_predecode_zero_scalars);
                    scalar_predecode_capacity.fetch_add(local_stats.scalar_predecode_capacity);

                    atomic_update_max(memory_row_buckets_max, local_stats.memory_fast64_row_buckets_max_bytes);
                    atomic_update_max(memory_bucket_masks_max, local_stats.memory_fast64_bucket_point_masks_max_bytes);
                    atomic_update_max(memory_nz_scalars_max, local_stats.memory_fast64_nz_scalars_max_bytes);
                    atomic_update_max(memory_first_hits_max, local_stats.memory_fast64_first_hits_max_bytes);
                    atomic_update_max(memory_static_workspace_max, local_stats.memory_fast64_static_workspace_max_bytes);
                    atomic_update_max(memory_peak_tile_max, local_stats.memory_fast64_estimated_peak_tile_max_bytes);
                    atomic_update_max(memory_window_events_max, local_stats.memory_fast64_window_events_max_bytes);

                    count_build_us.fetch_add(seconds_to_us(local_stats.count_build_seconds));
                    count_scan_us.fetch_add(seconds_to_us(local_stats.count_scan_seconds));
                    count_finalize_us.fetch_add(seconds_to_us(local_stats.count_finalize_seconds));
                    direct_window_us.fetch_add(seconds_to_us(local_stats.direct_window_seconds));
                    g1_group_build_us.fetch_add(seconds_to_us(local_stats.g1_group_build_seconds));
                    group_event_loop_us.fetch_add(seconds_to_us(local_stats.group_event_loop_seconds));
                    singleton_direct_us.fetch_add(seconds_to_us(local_stats.singleton_direct_seconds));
                    running_sum_us.fetch_add(seconds_to_us(local_stats.running_sum_seconds));
                    scalar_predecode_us.fetch_add(seconds_to_us(local_stats.scalar_predecode_seconds));
                    event_precompute_us.fetch_add(seconds_to_us(local_stats.event_precompute_seconds));
                    direct_bucket_accum_us.fetch_add(seconds_to_us(local_stats.direct_bucket_accum_seconds));
                    workspace_clear_us.fetch_add(seconds_to_us(local_stats.tile_workspace_clear_seconds));
                    workspace_init_us.fetch_add(seconds_to_us(local_stats.fast64_workspace_init_seconds));
                    const auto tm1 = std::chrono::steady_clock::now();
                    local_atomic_merge += elapsed_seconds(tm0, tm1);

                    local_count_build += local_stats.count_build_seconds;
                    local_g1_group_build += local_stats.g1_group_build_seconds;
                    local_running_sum += local_stats.running_sum_seconds;
                    local_workspace_init += local_stats.fast64_workspace_init_seconds;

                    const auto ts0 = std::chrono::steady_clock::now();
                    for (size_t r = 0; r < tile_rows; ++r) {
                        serialize_g1_row(tile_outputs[r], output, local_start + r);
                    }
                    const auto ts1 = std::chrono::steady_clock::now();
                    local_serialize += elapsed_seconds(ts0, ts1);

                    const size_t done = done_rows.fetch_add(tile_rows) + tile_rows;
                    if (opt.progress_every > 0 &&
                        (done >= opt.row_count || done % opt.progress_every < tile_rows)) {
                        std::lock_guard<std::mutex> guard(log_mutex);
                        std::cerr << "[bn254-msm] rows "
                                  << std::min(done, opt.row_count) << "/"
                                  << opt.row_count << " done (thread " << tid
                                  << ", " << opt.mode << ")\n";
                    }
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> guard(error_mutex);
                if (first_error.empty()) first_error = e.what();
            }

            const auto t_worker1 = std::chrono::steady_clock::now();
            ThreadWallTimeRecord& rec = thread_wall_times[tid];
            rec.count_build_seconds = local_count_build;
            rec.g1_group_build_seconds = local_g1_group_build;
            rec.running_sum_seconds = local_running_sum;
            rec.tile_scalar_alloc_seconds = local_scalar_alloc;
            rec.tile_scalar_read_seconds = local_scalar_read;
            rec.fast64_workspace_init_seconds = local_workspace_init;
            rec.tile_output_serialize_seconds = local_serialize;
            rec.stats_atomic_merge_seconds = local_atomic_merge;
            rec.tile_dispatch_wait_seconds = local_dispatch_wait;
            rec.total_worker_seconds = elapsed_seconds(t_worker0, t_worker1);

            scalar_alloc_us.fetch_add(seconds_to_us(local_scalar_alloc));
            scalar_read_us.fetch_add(seconds_to_us(local_scalar_read));
            serialize_us.fetch_add(seconds_to_us(local_serialize));
            atomic_merge_us.fetch_add(seconds_to_us(local_atomic_merge));
            dispatch_wait_us.fetch_add(seconds_to_us(local_dispatch_wait));
        };

        std::cerr << "[bn254-msm] computing rows=" << opt.row_count
                  << ", n=" << opt.n
                  << ", c=" << opt.c
                  << ", scalar_bits=" << opt.scalar_bits
                  << ", scalar_format=" << opt.scalar_format
                  << ", threads=" << opt.threads
                  << ", mode=" << opt.mode
                  << ", tile_size=" << opt.tile_size << "\n";

        std::vector<std::thread> threads;
        threads.reserve(opt.threads);
        for (unsigned t = 0; t < opt.threads; ++t) {
            threads.emplace_back(tile_worker, t);
        }
        for (std::thread& t : threads) t.join();

        if (!first_error.empty()) {
            throw std::runtime_error(first_error);
        }

        double max_worker_seconds = 0.0;
        for (const ThreadWallTimeRecord& rec : thread_wall_times) {
            max_worker_seconds = std::max(max_worker_seconds, rec.total_worker_seconds);
        }
        for (ThreadWallTimeRecord& rec : thread_wall_times) {
            rec.barrier_wait_seconds =
                std::max(0.0, max_worker_seconds - rec.total_worker_seconds);
            timing.barrier_wait_seconds += rec.barrier_wait_seconds;
        }

        const auto t_compute1 = std::chrono::steady_clock::now();
        const double compute_wall_including_scalar_read =
            elapsed_seconds(t_compute0, t_compute1);

        timing.bucket_point_adds = bucket_point_adds.load();
        timing.raw_row_bucket_adds = raw_row_bucket_adds.load();
        timing.optimized_bucket_adds_est = optimized_bucket_adds_est.load();
        timing.exact_mask_saving_est = exact_mask_saving_est.load();
        timing.shared_group_point_count = shared_group_point_count.load();
        timing.fast_direct_row_bucket_adds = fast_direct_row_bucket_adds.load();
        timing.shared_bucket_groups = shared_bucket_groups.load();
        timing.shared_bucket_row_links = shared_bucket_row_links.load();
        timing.group_point_visits = group_point_visits.load();
        timing.share_point_events = share_point_events.load();
        timing.fallback_point_events = fallback_point_events.load();
        timing.fallback_row_adds = fallback_row_adds.load();
        timing.fallback_popcount1_events = fallback_popcount1_events.load();
        timing.shared_link_row_adds = shared_link_row_adds.load();
        timing.single_leaf_fast_slots = single_leaf_fast_slots.load();
        timing.multi_leaf_fast_slots = multi_leaf_fast_slots.load();
        timing.shareable_mask_entries = shareable_mask_entries.load();
        timing.shareable_mask_hits = shareable_mask_hits.load();
        timing.nonshareable_mask_misses = nonshareable_mask_misses.load();
        timing.singleton_direct_point_events = singleton_direct_point_events.load();
        timing.singleton_direct_row_adds = singleton_direct_row_adds.load();
        timing.multiscale_parent_chosen = multiscale_parent_chosen.load();
        timing.multiscale_child_chosen = multiscale_child_chosen.load();
        timing.zero_tile_skipped_count = zero_tile_skipped_count.load();

        timing.scalar_predecode_tiles = scalar_predecode_tiles.load();
        timing.scalar_predecode_nonzero_scalars = scalar_predecode_nonzero_scalars.load();
        timing.scalar_predecode_zero_scalars = scalar_predecode_zero_scalars.load();
        timing.scalar_predecode_capacity = scalar_predecode_capacity.load();

        timing.memory_fast64_row_buckets_max_bytes = memory_row_buckets_max.load();
        timing.memory_fast64_bucket_point_masks_max_bytes = memory_bucket_masks_max.load();
        timing.memory_fast64_nz_scalars_max_bytes = memory_nz_scalars_max.load();
        timing.memory_fast64_first_hits_max_bytes = memory_first_hits_max.load();
        timing.memory_fast64_static_workspace_max_bytes = memory_static_workspace_max.load();
        timing.memory_fast64_estimated_peak_tile_max_bytes = memory_peak_tile_max.load();
        timing.memory_fast64_window_events_max_bytes = memory_window_events_max.load();

        timing.tile_count_build_seconds = static_cast<double>(count_build_us.load()) / 1e6;
        timing.tile_count_scan_seconds = static_cast<double>(count_scan_us.load()) / 1e6;
        timing.tile_count_finalize_seconds = static_cast<double>(count_finalize_us.load()) / 1e6;
        timing.tile_direct_window_seconds = static_cast<double>(direct_window_us.load()) / 1e6;
        timing.tile_g1_group_build_seconds = static_cast<double>(g1_group_build_us.load()) / 1e6;
        timing.tile_group_event_loop_seconds = static_cast<double>(group_event_loop_us.load()) / 1e6;
        timing.tile_singleton_direct_seconds = static_cast<double>(singleton_direct_us.load()) / 1e6;
        timing.tile_running_sum_seconds = static_cast<double>(running_sum_us.load()) / 1e6;
        timing.scalar_predecode_seconds = static_cast<double>(scalar_predecode_us.load()) / 1e6;
        timing.event_precompute_seconds = static_cast<double>(event_precompute_us.load()) / 1e6;
        timing.direct_bucket_accum_seconds = static_cast<double>(direct_bucket_accum_us.load()) / 1e6;
        timing.tile_workspace_clear_seconds = static_cast<double>(workspace_clear_us.load()) / 1e6;
        timing.fast64_workspace_init_seconds = static_cast<double>(workspace_init_us.load()) / 1e6;
        timing.tile_scalar_alloc_seconds = static_cast<double>(scalar_alloc_us.load()) / 1e6;
        timing.tile_scalar_read_seconds = static_cast<double>(scalar_read_us.load()) / 1e6;
        timing.tile_output_serialize_seconds = static_cast<double>(serialize_us.load()) / 1e6;
        timing.stats_atomic_merge_seconds = static_cast<double>(atomic_merge_us.load()) / 1e6;
        timing.tile_dispatch_wait_seconds = static_cast<double>(dispatch_wait_us.load()) / 1e6;
        timing.thread_wall_times = thread_wall_times;

        timing.tile_direct_sparse_seconds = (opt.mode == "pippenger")
            ? timing.scalar_predecode_seconds +
                  timing.direct_bucket_accum_seconds +
                  timing.tile_running_sum_seconds
            : 0.0;

        // Sum of worker-attributed compute phases; scalar file I/O is separate.
        timing.compute_seconds = (opt.mode == "pippenger")
            ? timing.scalar_predecode_seconds +
                  timing.direct_bucket_accum_seconds +
                  timing.tile_running_sum_seconds
            : timing.tile_count_build_seconds +
                  timing.scalar_predecode_seconds +
                  timing.event_precompute_seconds +
                  timing.tile_g1_group_build_seconds +
                  timing.tile_running_sum_seconds;

        std::cerr << "[bn254-msm] writing output " << opt.out_path << "\n";
        const auto t_write0 = std::chrono::steady_clock::now();
        write_npy_u64_2d(opt.out_path, output, opt.row_count, 4);
        const auto t_write1 = std::chrono::steady_clock::now();
        timing.write_output_seconds = elapsed_seconds(t_write0, t_write1);

        const auto t_total1 = std::chrono::steady_clock::now();
        timing.total_seconds = elapsed_seconds(t_total0, t_total1);
        write_stats(opt, timing);

        std::cerr << "[bn254-msm] finished in " << std::fixed
                  << std::setprecision(3) << timing.total_seconds << "s"
                  << " (init=" << timing.init_seconds
                  << "s, load_points=" << timing.load_points_seconds
                  << "s, scalar_header=" << timing.scalar_header_seconds
                  << "s, scalar_read_thread_sum=" << timing.tile_scalar_read_seconds
                  << "s, compute_thread_sum=" << timing.compute_seconds
                  << "s, compute_wall_including_scalar_read="
                  << compute_wall_including_scalar_read
                  << "s, write=" << timing.write_output_seconds << "s)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[bn254-msm] error: " << e.what() << "\n";
        return 1;
    }
}

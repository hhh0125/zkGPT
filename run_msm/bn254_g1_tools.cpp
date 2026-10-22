#include <mcl/bn.hpp>

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using mcl::bn::Fr;
using mcl::bn::G1;

namespace {

struct NpyHeader {
    std::string descr;
    bool fortran_order = false;
    std::vector<size_t> shape;
    std::streamoff data_offset = 0;
};

struct Options {
    std::string mode;
    std::string input;
    std::string output;
    std::string seed;
    size_t count = 0;
    bool append_base = true;
    bool require_nonzero = true;
};

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
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string extract_quoted_value(const std::string& header, const std::string& key) {
    size_t pos = header.find("'" + key + "'");
    if (pos == std::string::npos) pos = header.find("\"" + key + "\"");
    if (pos == std::string::npos) throw std::runtime_error("NPY header missing key: " + key);
    pos = header.find(':', pos);
    size_t quote = header.find_first_of("'\"", pos + 1);
    if (pos == std::string::npos || quote == std::string::npos) {
        throw std::runtime_error("Malformed NPY header for key: " + key);
    }
    char q = header[quote];
    size_t end = header.find(q, quote + 1);
    if (end == std::string::npos) throw std::runtime_error("Unterminated NPY header value: " + key);
    return header.substr(quote + 1, end - quote - 1);
}

bool extract_bool_value(const std::string& header, const std::string& key) {
    size_t pos = header.find("'" + key + "'");
    if (pos == std::string::npos) pos = header.find("\"" + key + "\"");
    if (pos == std::string::npos) throw std::runtime_error("NPY header missing key: " + key);
    pos = header.find(':', pos);
    size_t end = header.find(',', pos + 1);
    std::string value = trim(header.substr(pos + 1, end - pos - 1));
    if (value == "False") return false;
    if (value == "True") return true;
    throw std::runtime_error("Unsupported NPY bool value: " + value);
}

std::vector<size_t> extract_shape(const std::string& header) {
    size_t key = header.find("'shape'");
    if (key == std::string::npos) key = header.find("\"shape\"");
    if (key == std::string::npos) throw std::runtime_error("NPY header missing shape");
    size_t l = header.find('(', key);
    size_t r = header.find(')', l);
    if (l == std::string::npos || r == std::string::npos || r <= l) {
        throw std::runtime_error("Malformed NPY shape");
    }
    std::vector<size_t> shape;
    std::stringstream ss(header.substr(l + 1, r - l - 1));
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (!token.empty()) shape.push_back(static_cast<size_t>(std::stoull(token)));
    }
    return shape;
}

NpyHeader read_npy_header(std::ifstream& in, const std::string& path) {
    char magic[6];
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, "\x93NUMPY", 6) != 0) throw std::runtime_error(path + " is not .npy");

    unsigned char version[2];
    in.read(reinterpret_cast<char*>(version), sizeof(version));
    uint32_t header_len = 0;
    if (version[0] == 1) {
        unsigned char b[2];
        in.read(reinterpret_cast<char*>(b), sizeof(b));
        header_len = load_u16_le(b);
    } else if (version[0] == 2 || version[0] == 3) {
        unsigned char b[4];
        in.read(reinterpret_cast<char*>(b), sizeof(b));
        header_len = load_u32_le(b);
    } else {
        throw std::runtime_error("Unsupported NPY version");
    }

    std::string header(header_len, '\0');
    in.read(&header[0], header.size());
    if (!in) throw std::runtime_error("Failed to read NPY header");

    NpyHeader out;
    out.descr = extract_quoted_value(header, "descr");
    out.fortran_order = extract_bool_value(header, "fortran_order");
    out.shape = extract_shape(header);
    out.data_offset = in.tellg();
    if (out.descr != "<u8" && out.descr != "|u8") throw std::runtime_error("Expected uint64 npy");
    if (out.fortran_order) throw std::runtime_error("Fortran-order npy is not supported");
    return out;
}

std::string make_npy_header(size_t rows, size_t cols) {
    std::ostringstream oss;
    oss << "{'descr': '<u8', 'fortran_order': False, 'shape': (" << rows << ", " << cols << "), }";
    std::string header = oss.str();
    header.push_back('\n');
    while ((10 + header.size()) % 16 != 0) header.insert(header.end() - 1, ' ');
    return header;
}

void write_npy_u64_2d(const std::string& path, const std::vector<uint64_t>& data, size_t rows, size_t cols) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open output: " + path);
    const char magic[] = "\x93NUMPY";
    out.write(magic, 6);
    const unsigned char version[2] = {1, 0};
    out.write(reinterpret_cast<const char*>(version), 2);
    std::string header = make_npy_header(rows, cols);
    uint16_t hlen = static_cast<uint16_t>(header.size());
    unsigned char hlen_bytes[2] = {
        static_cast<unsigned char>(hlen & 0xff),
        static_cast<unsigned char>((hlen >> 8) & 0xff),
    };
    out.write(reinterpret_cast<const char*>(hlen_bytes), 2);
    out.write(header.data(), header.size());
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(uint64_t)));
    if (!out) throw std::runtime_error("Failed while writing output: " + path);
}

std::string bytes_to_hex(const unsigned char *buf, size_t n) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i) oss << std::setw(2) << static_cast<unsigned>(buf[i]);
    return oss.str();
}

std::string limbs_to_string(const uint64_t *row) {
    std::ostringstream oss;
    oss << "[" << row[0] << ", " << row[1] << ", " << row[2] << ", " << row[3] << "]";
    return oss.str();
}

bool read_bool_arg(const std::map<std::string, std::string>& args, const std::string& name, bool def) {
    auto it = args.find(name);
    if (it == args.end()) return def;
    std::string v = it->second;
    for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (v == "1" || v == "true" || v == "yes") return true;
    if (v == "0" || v == "false" || v == "no") return false;
    throw std::runtime_error("Invalid bool value for " + name + ": " + it->second);
}

Options parse_args(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) != 0 || i + 1 >= argc) throw std::runtime_error("Expected --key value arguments");
        args[key] = argv[++i];
    }
    Options opt;
    if (args.count("--mode")) opt.mode = args["--mode"];
    if (args.count("--input")) opt.input = args["--input"];
    if (args.count("--out")) opt.output = args["--out"];
    if (args.count("--count")) opt.count = static_cast<size_t>(std::stoull(args["--count"]));
    if (args.count("--seed")) opt.seed = args["--seed"];
    opt.append_base = read_bool_arg(args, "--append-base", true);
    opt.require_nonzero = read_bool_arg(args, "--require-nonzero", true);
    if (opt.mode != "generate" && opt.mode != "validate") {
        throw std::runtime_error("--mode must be generate or validate");
    }
    if (opt.mode == "generate" && (opt.output.empty() || opt.count == 0)) {
        throw std::runtime_error("generate requires --out and --count");
    }
    if (opt.mode == "validate" && opt.input.empty()) {
        throw std::runtime_error("validate requires --input");
    }
    return opt;
}

Fr scalar_for_index(size_t i, const std::string& seed) {
    Fr s;
    if (seed.empty()) {
        s.setByCSPRNG();
    } else {
        std::string msg = seed + ":" + std::to_string(i);
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

void generate_points(const Options& opt) {
    auto t0 = std::chrono::steady_clock::now();
    const G1 base = bn254_explicit_base_point();
    const size_t random_count = opt.append_base ? opt.count - 1 : opt.count;
    std::vector<uint64_t> mat;
    mat.reserve(opt.count * 4);
    unsigned char buf[32];

    for (size_t i = 0; i < random_count; ++i) {
        Fr s = scalar_for_index(i, opt.seed);
        G1 p = base * s;
        const size_t written = p.serialize(buf, sizeof(buf), mcl::IoSerialize);
        if (written != sizeof(buf)) throw std::runtime_error("G1 serialize failed at generated point " + std::to_string(i));
        for (int j = 0; j < 4; ++j) {
            uint64_t v = 0;
            std::memcpy(&v, buf + j * 8, 8);
            mat.push_back(v);
        }
    }
    if (opt.append_base) {
        const size_t written = base.serialize(buf, sizeof(buf), mcl::IoSerialize);
        if (written != sizeof(buf)) throw std::runtime_error("G1 serialize failed for base point");
        for (int j = 0; j < 4; ++j) {
            uint64_t v = 0;
            std::memcpy(&v, buf + j * 8, 8);
            mat.push_back(v);
        }
    }

    write_npy_u64_2d(opt.output, mat, opt.count, 4);
    auto t1 = std::chrono::steady_clock::now();
    std::cerr << "[bn254-g1] generated " << opt.count << " points to " << opt.output
              << " in " << std::fixed << std::setprecision(3)
              << std::chrono::duration<double>(t1 - t0).count() << "s";
    if (!opt.seed.empty()) std::cerr << " (seed=" << opt.seed << ")";
    std::cerr << "\n";
}

int validate_points(const Options& opt) {
    auto t0 = std::chrono::steady_clock::now();
    std::ifstream in(opt.input, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open input: " + opt.input);
    NpyHeader h = read_npy_header(in, opt.input);
    if (h.shape.size() != 2 || h.shape[1] != 4) throw std::runtime_error("Expected shape (count, 4)");

    size_t ok = 0;
    size_t infinity = 0;
    size_t bad = 0;
    size_t first_bad = static_cast<size_t>(-1);
    uint64_t first_bad_row[4] = {0, 0, 0, 0};
    unsigned char first_bad_buf[32] = {0};

    in.seekg(h.data_offset);
    uint64_t row[4];
    unsigned char buf[32];
    for (size_t i = 0; i < h.shape[0]; ++i) {
        in.read(reinterpret_cast<char*>(row), sizeof(row));
        if (!in) throw std::runtime_error("Failed to read row " + std::to_string(i));
        std::memcpy(buf, row, sizeof(buf));
        G1 p;
        size_t nread = p.deserialize(buf, sizeof(buf), mcl::IoSerialize);
        if (nread == sizeof(buf)) {
            ok++;
            if (p.isZero()) infinity++;
        } else {
            bad++;
            if (first_bad == static_cast<size_t>(-1)) {
                first_bad = i;
                std::memcpy(first_bad_row, row, sizeof(row));
                std::memcpy(first_bad_buf, buf, sizeof(buf));
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    std::cerr << "[bn254-g1] validate " << opt.input
              << ": rows=" << h.shape[0]
              << ", ok=" << ok
              << ", infinity=" << infinity
              << ", bad=" << bad
              << ", seconds=" << std::fixed << std::setprecision(3)
              << std::chrono::duration<double>(t1 - t0).count() << "\n";
    if (bad > 0) {
        std::cerr << "[bn254-g1] first_bad_index=" << first_bad
                  << ", uint64_limbs=" << limbs_to_string(first_bad_row)
                  << ", bytes_hex=" << bytes_to_hex(first_bad_buf, sizeof(first_bad_buf)) << "\n";
    }
    if (opt.require_nonzero && infinity > 0) {
        std::cerr << "[bn254-g1] error: infinity points found while --require-nonzero=true\n";
        return 2;
    }
    return bad == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        Options opt = parse_args(argc, argv);
        mcl::bn::initPairing(mcl::BN254);
        if (opt.mode == "generate") {
            generate_points(opt);
            return 0;
        }
        return validate_points(opt);
    } catch (const std::exception& e) {
        std::cerr << "[bn254-g1] error: " << e.what() << "\n";
        return 1;
    }
}

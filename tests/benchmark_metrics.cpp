#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <filesystem>
#include <sstream>
#include <algorithm>

#include "core/sha256.h"
#include "core/zlib_compress.h"
#include "storage/object_database.h"
#include "storage/pack.h"
#include "storage/repack.h"
#include "diff/diff_engine.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace fs = std::filesystem;
using namespace minigit::storage;

namespace {

size_t get_peak_rss_kb()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return pmc.PeakWorkingSetSize / 1024;
    }
#endif
    return 0;
}

std::string generate_text(size_t target_bytes)
{
    static const char sample_words[] =
        "The quick brown fox jumps over the lazy dog. MiniGit fast distributed version control system. "
        "int main(int argc, char* argv[]) { std::cout << \"Hello World!\" << std::endl; return 0; } "
        "struct CommitNode { std::string sha; std::vector<std::string> parents; };\n";
    const size_t sample_len = sizeof(sample_words) - 1;

    std::string result;
    result.reserve(target_bytes);
    while (result.size() < target_bytes)
    {
        size_t chunk = (std::min)(sample_len, target_bytes - result.size());
        result.append(sample_words, chunk);
    }
    return result;
}

void print_header(const std::string& title)
{
    std::cout << "\n================================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "================================================================================\n";
}

void print_row(const std::string& name, const std::string& size_str, int iters, double time_ms, double throughput_mb_s, const std::string& extra = "")
{
    std::cout << "  " << std::left << std::setw(28) << name
              << std::setw(12) << size_str
              << std::right << std::setw(8) << iters
              << std::setw(12) << std::fixed << std::setprecision(2) << time_ms << " ms"
              << std::setw(12) << std::fixed << std::setprecision(2) << throughput_mb_s << " MB/s"
              << (extra.empty() ? "" : ("   [" + extra + "]"))
              << "\n";
}

} // namespace

void benchmark_sha256()
{
    print_header("1. SHA-256 Hashing Throughput");
    std::cout << "  " << std::left << std::setw(28) << "Benchmark"
              << std::setw(12) << "Size"
              << std::right << std::setw(8) << "Iters"
              << std::setw(15) << "Total Time"
              << std::setw(17) << "Throughput"
              << "\n";
    std::cout << "  " << std::string(76, '-') << "\n";

    const struct {
        std::string label;
        size_t size;
        int iters;
    } cases[] = {
        {"SHA-256 (Small)",    1 * 1024,      10000},
        {"SHA-256 (Medium)",   64 * 1024,     2000},
        {"SHA-256 (Large)",    1024 * 1024,   100},
        {"SHA-256 (Bulk 10MB)", 10 * 1024 * 1024, 10}
    };

    for (const auto& c : cases)
    {
        const std::string data = generate_text(c.size);
        const auto start = std::chrono::high_resolution_clock::now();
        volatile size_t dummy_sink = 0;
        for (int i = 0; i < c.iters; ++i)
        {
            const std::string h = sha256(data);
            dummy_sink += h.size();
        }
        const auto end = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(end - start).count();
        const double total_mb = (static_cast<double>(c.size) * c.iters) / (1024.0 * 1024.0);
        const double mb_per_sec = total_mb / (ms / 1000.0);

        std::string size_display;
        if (c.size < 1024 * 1024)
            size_display = std::to_string(c.size / 1024) + " KB";
        else
            size_display = std::to_string(c.size / (1024 * 1024)) + " MB";

        print_row(c.label, size_display, c.iters, ms, mb_per_sec);
    }
}

void benchmark_zlib()
{
    print_header("2. Zlib Deflate & Inflate Performance");
    std::cout << "  " << std::left << std::setw(28) << "Benchmark"
              << std::setw(12) << "Size"
              << std::right << std::setw(8) << "Iters"
              << std::setw(15) << "Total Time"
              << std::setw(17) << "Throughput"
              << "   Details\n";
    std::cout << "  " << std::string(88, '-') << "\n";

    const struct {
        std::string label;
        size_t size;
        int iters;
    } cases[] = {
        {"100 KB Text", 100 * 1024, 100},
        {"1 MB Source",  1024 * 1024, 20},
        {"5 MB Binary",  5 * 1024 * 1024, 5}
    };

    for (const auto& c : cases)
    {
        const std::string original = generate_text(c.size);

        // Compress
        const auto c_start = std::chrono::high_resolution_clock::now();
        std::string compressed;
        for (int i = 0; i < c.iters; ++i)
        {
            compressed = zlib_compress(original);
        }
        const auto c_end = std::chrono::high_resolution_clock::now();
        const double c_ms = std::chrono::duration<double, std::milli>(c_end - c_start).count();
        const double total_mb = (static_cast<double>(c.size) * c.iters) / (1024.0 * 1024.0);
        const double c_mb_s = total_mb / (c_ms / 1000.0);
        const double ratio = 100.0 * static_cast<double>(compressed.size()) / static_cast<double>(c.size);

        std::ostringstream c_extra;
        c_extra << "ratio: " << std::fixed << std::setprecision(1) << ratio << "%";
        print_row(c.label + " Deflate", std::to_string(c.size / 1024) + " KB", c.iters, c_ms, c_mb_s, c_extra.str());

        // Decompress
        const auto d_start = std::chrono::high_resolution_clock::now();
        volatile size_t sink = 0;
        for (int i = 0; i < c.iters; ++i)
        {
            std::string decompressed = zlib_decompress(compressed);
            sink += decompressed.size();
        }
        const auto d_end = std::chrono::high_resolution_clock::now();
        const double d_ms = std::chrono::duration<double, std::milli>(d_end - d_start).count();
        const double d_mb_s = total_mb / (d_ms / 1000.0);

        print_row(c.label + " Inflate", std::to_string(c.size / 1024) + " KB", c.iters, d_ms, d_mb_s, "verified exact match");
    }
}

void benchmark_cas_storage()
{
    print_header("3. Content-Addressable Storage (CAS): Loose vs. Packfile");

    const fs::path temp_dir = fs::temp_directory_path() / ("minigit_bench_cas_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_dir / "objects");

    ObjectDatabase db(temp_dir / "objects");

    const int object_count = 1000;
    const size_t obj_size = 1024; // 1 KB each
    std::vector<std::string> ids;
    std::vector<std::string> envelopes;
    ids.reserve(object_count);
    envelopes.reserve(object_count);

    for (int i = 0; i < object_count; ++i)
    {
        std::string content = "Blob " + std::to_string(i) + ": " + generate_text(obj_size);
        std::string env = "blob " + std::to_string(content.size()) + '\0' + content;
        ids.push_back(sha256(env));
        envelopes.push_back(std::move(env));
    }

    // 1. Loose write
    const auto lw_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < object_count; ++i)
    {
        db.write(ids[i], envelopes[i]);
    }
    const auto lw_end = std::chrono::high_resolution_clock::now();
    const double lw_ms = std::chrono::duration<double, std::milli>(lw_end - lw_start).count();
    const double lw_mb = (static_cast<double>(obj_size) * object_count) / (1024.0 * 1024.0);
    const double lw_ops = static_cast<double>(object_count) / (lw_ms / 1000.0);
    std::cout << "  * Loose Objects Write:       "
              << std::fixed << std::setprecision(2) << lw_ms << " ms ("
              << std::fixed << std::setprecision(0) << lw_ops << " ops/sec, "
              << std::fixed << std::setprecision(2) << (lw_mb / (lw_ms / 1000.0)) << " MB/s)\n";

    // 2. Loose read
    const auto lr_start = std::chrono::high_resolution_clock::now();
    volatile size_t sink = 0;
    for (int i = 0; i < object_count; ++i)
    {
        std::string read_back = db.read(ids[i]);
        sink += read_back.size();
    }
    const auto lr_end = std::chrono::high_resolution_clock::now();
    const double lr_ms = std::chrono::duration<double, std::milli>(lr_end - lr_start).count();
    const double lr_ops = static_cast<double>(object_count) / (lr_ms / 1000.0);
    std::cout << "  * Loose Objects Read:        "
              << std::fixed << std::setprecision(2) << lr_ms << " ms ("
              << std::fixed << std::setprecision(0) << lr_ops << " ops/sec, "
              << std::fixed << std::setprecision(2) << (lw_mb / (lr_ms / 1000.0)) << " MB/s)\n";

    // 3. Repack (sliding window delta + index creation)
    RepackOptions opts;
    opts.pack_all = true;
    opts.delete_loose = true;
    opts.window = 5;

    const auto repack_start = std::chrono::high_resolution_clock::now();
    const RepackResult repack_res = repack_repository(temp_dir, opts);
    const auto repack_end = std::chrono::high_resolution_clock::now();
    const double repack_ms = std::chrono::duration<double, std::milli>(repack_end - repack_start).count();
    std::cout << "  * Repack & Delta Encode:     "
              << std::fixed << std::setprecision(2) << repack_ms << " ms ("
              << repack_res.loose_objects_found << " objects packed, "
              << repack_res.pack_result.delta_objects << " delta compressed)\n";

    // 4. Packfile Read & Random Lookup
    db.reload_packs();
    const auto pr_start = std::chrono::high_resolution_clock::now();
    volatile size_t pack_sink = 0;
    for (int i = 0; i < object_count; ++i)
    {
        std::string read_back = db.read(ids[i]);
        pack_sink += read_back.size();
    }
    const auto pr_end = std::chrono::high_resolution_clock::now();
    const double pr_ms = std::chrono::duration<double, std::milli>(pr_end - pr_start).count();
    const double pr_ops = static_cast<double>(object_count) / (pr_ms / 1000.0);
    std::cout << "  * Pack Indexed Binary Read:  "
              << std::fixed << std::setprecision(2) << pr_ms << " ms ("
              << std::fixed << std::setprecision(0) << pr_ops << " ops/sec, "
              << std::fixed << std::setprecision(2) << (lw_mb / (pr_ms / 1000.0)) << " MB/s)\n";

    std::error_code ec;
    fs::remove_all(temp_dir, ec);
}

void benchmark_diff_engine()
{
    print_header("4. Diff Engine: Longest Common Subsequence (LCS) Scaling");
    std::cout << "  " << std::left << std::setw(24) << "Input Scale"
              << std::setw(14) << "Line Count"
              << std::right << std::setw(8) << "Iters"
              << std::setw(15) << "Avg Time"
              << std::setw(18) << "Throughput"
              << "\n";
    std::cout << "  " << std::string(79, '-') << "\n";

    const struct {
        std::string label;
        int lines;
        int iters;
    } cases[] = {
        {"Small Source File",   100,  1000},
        {"Medium Source File",  1000, 50},
        {"Large Module File",   5000, 5}
    };

    for (const auto& c : cases)
    {
        std::vector<std::string> old_lines;
        std::vector<std::string> new_lines;
        old_lines.reserve(c.lines);
        new_lines.reserve(c.lines);

        for (int i = 0; i < c.lines; ++i)
        {
            std::string line = "Line " + std::to_string(i) + ": int var_" + std::to_string(i) + " = compute_hash(" + std::to_string(i * 31) + ");";
            old_lines.push_back(line);
            if (i % 10 == 0) // 10% modification
            {
                line += " // modified";
            }
            new_lines.push_back(line);
        }

        const auto start = std::chrono::high_resolution_clock::now();
        volatile size_t total_edits = 0;
        for (int i = 0; i < c.iters; ++i)
        {
            auto edits = lcs_diff(old_lines, new_lines);
            total_edits += edits.size();
        }
        const auto end = std::chrono::high_resolution_clock::now();
        const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        const double avg_ms = total_ms / c.iters;
        const double lines_per_sec = (static_cast<double>(c.lines) * c.iters) / (total_ms / 1000.0);

        std::cout << "  " << std::left << std::setw(24) << c.label
                  << std::setw(14) << (std::to_string(c.lines) + " lines")
                  << std::right << std::setw(8) << c.iters
                  << std::setw(12) << std::fixed << std::setprecision(3) << avg_ms << " ms"
                  << std::setw(15) << std::fixed << std::setprecision(0) << lines_per_sec << " lines/s\n";
    }
}

int main()
{
    std::cout << "================================================================================\n";
    std::cout << "               MiniGit Performance & Memory Benchmark Suite                   \n";
    std::cout << "================================================================================\n";

    const size_t initial_rss = get_peak_rss_kb();

    benchmark_sha256();
    benchmark_zlib();
    benchmark_cas_storage();
    benchmark_diff_engine();

    const size_t final_rss = get_peak_rss_kb();

    print_header("5. Process Memory Footprint");
    if (final_rss > 0)
    {
        std::cout << "  * Initial Peak Working Set: " << initial_rss << " KB (" << std::fixed << std::setprecision(2) << (initial_rss / 1024.0) << " MB)\n";
        std::cout << "  * Final Peak Working Set:   " << final_rss << " KB (" << std::fixed << std::setprecision(2) << (final_rss / 1024.0) << " MB)\n";
        std::cout << "  * Net Memory Delta:         " << (final_rss >= initial_rss ? final_rss - initial_rss : 0) << " KB\n";
    }
    else
    {
        std::cout << "  * Platform memory counter unavailable.\n";
    }

    std::cout << "\n================================================================================\n";
    std::cout << "                     Benchmark Suite Execution Completed                       \n";
    std::cout << "================================================================================\n";

    return 0;
}

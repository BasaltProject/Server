#include <basalt/metamorph/region/BswRegion.hpp>
#include <basalt/metamorph/region/ChunkBlob.hpp>
#include <basalt/metamorph/region/LinearRegion.hpp>
#include <basalt/server/world/region/ChunkBlob.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <malloc.h>
#include <string>
#include <vector>
#include <zstd.h>

namespace fs = std::filesystem;
using namespace basalt;
using Clock = std::chrono::steady_clock;

namespace {

double since(Clock::time_point t) { return std::chrono::duration<double, std::micro>(Clock::now() - t).count(); }

struct Acc {
    double v = 0;

    void add(double x) { v += x; }
};

} // namespace

int main(int argc, char** argv) {
    const std::string folder = argc > 1 ? argv[1] : "region";
    std::vector<std::vector<std::uint8_t>> blobs;
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.path().extension() != ".linear") {
            continue;
        }
        const auto bytes = wire::io::fileIO::readBytes(entry.path().string());
        auto region = metamorph::region::LinearRegion::read(*bytes);
        if (!region) {
            continue;
        }
        for (int z = 0; z < 32; ++z) {
            for (int x = 0; x < 32; ++x) {
                const auto c = region->chunkAt(x, z);
                if (!c.empty()) {
                    blobs.emplace_back(c.begin(), c.end());
                }
            }
        }
    }
    std::printf("chunks %zu\n", blobs.size());
    std::vector<std::span<const std::uint8_t>> views(blobs.begin(), blobs.end());
    std::vector<std::span<const std::uint8_t>> sample(
        views.begin(), views.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(192, views.size())));
    auto dict = metamorph::region::BswDictionary::train(sample, 112640, 3);
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    ZSTD_CDict* cdict = ZSTD_createCDict(dict->bytes().data(), dict->bytes().size(), 3);
    ZSTD_DDict* ddict = ZSTD_createDDict(dict->bytes().data(), dict->bytes().size());

    Acc parse, build, enc, ser, zc, zd, secEnc, secZc;
    double rawBytes = 0, zBytes = 0, secZBytes = 0, sections = 0, uniform = 0;
    std::vector<std::uint8_t> comp(1 << 20), plain(1 << 22);
    const int rounds = 3;
    std::vector<server::world::Chunk> keep;
    for (int round = 0; round < rounds; ++round) {
        for (const auto& blob : blobs) {
            auto t = Clock::now();
            auto tag = wire::nbt::nbtIO::read(blob);
            parse.add(since(t));
            const auto* root = tag->tag.get<wire::nbt::NbtCompound>();
            t = Clock::now();
            auto chunk = server::world::region::chunkNbt::read(*root, server::world::kOverworldHeight);
            build.add(since(t));
            if (!chunk) {
                continue;
            }
            t = Clock::now();
            auto out = server::world::region::chunkNbt::write(*chunk);
            enc.add(since(t));
            t = Clock::now();
            auto bytes = wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(out));
            ser.add(since(t));
            t = Clock::now();
            const size_t n =
                ZSTD_compress_usingCDict(cctx, comp.data(), comp.size(), bytes.data(), bytes.size(), cdict);
            zc.add(since(t));
            t = Clock::now();
            ZSTD_decompress_usingDDict(dctx, plain.data(), plain.size(), comp.data(), n, ddict);
            zd.add(since(t));
            rawBytes += static_cast<double>(bytes.size());
            zBytes += static_cast<double>(n);
            if (round == 0) {
                const auto* list = out.getList("sections");
                for (const auto& s : *list) {
                    t = Clock::now();
                    auto sb = wire::nbt::nbtIO::write(s);
                    secEnc.add(since(t));
                    t = Clock::now();
                    const size_t sn =
                        ZSTD_compress_usingCDict(cctx, comp.data(), comp.size(), sb.data(), sb.size(), cdict);
                    secZc.add(since(t));
                    secZBytes += static_cast<double>(sn);
                    sections += 1;
                    if (sb.size() < 100) {
                        uniform += 1;
                    }
                }
            }
        }
    }
    const double n = static_cast<double>(blobs.size() * rounds);
    std::printf("per chunk us: nbt-parse %.0f  chunk-build %.0f  chunk-write(nbt tree) %.0f  nbt-serialize %.0f  "
                "zstd-c(dict) %.0f  zstd-d(dict) %.0f\n",
                parse.v / n, build.v / n, enc.v / n, ser.v / n, zc.v / n, zd.v / n);
    std::printf("avg raw nbt %.0f B, avg zstd %.0f B per chunk\n", rawBytes / n, zBytes / n);
    const double c1 = static_cast<double>(blobs.size());
    std::printf("sections/chunk %.1f, tiny(<100B nbt) %.1f/chunk, per-section: nbt-ser %.1f us, zstd-c %.1f us, avg "
                "zstd bytes %.0f, sum per chunk %.0f B (vs whole %.0f)\n",
                sections / c1, uniform / c1, secEnc.v / sections, secZc.v / sections, secZBytes / sections,
                secZBytes / c1, zBytes / n);
    {
        std::array<double, 10> bh{}, ih{};
        double secs = 0;
        for (const auto& blob : blobs) {
            auto tag = wire::nbt::nbtIO::read(blob);
            for (const auto& sec : *tag->tag.get<wire::nbt::NbtCompound>()->getList("sections")) {
                const auto* sc = sec.get<wire::nbt::NbtCompound>();
                const size_t bp = sc->getCompound("block_states")->getList("palette")->size();
                const size_t ip = sc->getCompound("biomes")->getList("palette")->size();
                int bb = 0;
                while ((size_t{1} << bb) < bp)
                    ++bb;
                int ib = 0;
                while ((size_t{1} << ib) < ip)
                    ++ib;
                bh[static_cast<size_t>(std::min(bb, 9))] += 1;
                ih[static_cast<size_t>(std::min(ib, 9))] += 1;
                secs += 1;
            }
        }
        std::printf("block palette by ceil-log2 bits (share of sections):");
        for (int i = 0; i < 10; ++i)
            std::printf(" %d:%.1f%%", i, 100 * bh[static_cast<size_t>(i)] / secs);
        std::printf("\nbiome palette bits:");
        for (int i = 0; i < 5; ++i)
            std::printf(" %d:%.1f%%", i, 100 * ih[static_cast<size_t>(i)] / secs);
        std::printf("\n");
    }
    {
        namespace cb = server::world::region::chunkBlob;
        Acc bw, br, bz, bd, bn, bf;
        double blobRaw = 0, blobZ = 0, mism = 0, mismNbt = 0;
        std::vector<std::uint8_t> keepBlob;
        for (int round = 0; round < 3; ++round) {
            for (const auto& nbt : blobs) {
                auto tag = wire::nbt::nbtIO::read(nbt);
                const auto* root = tag->tag.get<wire::nbt::NbtCompound>();
                auto chunk = server::world::region::chunkNbt::read(*root, server::world::kOverworldHeight);
                auto t = Clock::now();
                auto blob = cb::write(*chunk);
                bw.add(since(t));
                t = Clock::now();
                auto back = cb::read(*blob, server::world::kOverworldHeight);
                br.add(since(t));
                t = Clock::now();
                const size_t zn =
                    ZSTD_compress_usingCDict(cctx, comp.data(), comp.size(), blob->data(), blob->size(), cdict);
                bz.add(since(t));
                t = Clock::now();
                ZSTD_decompress_usingDDict(dctx, plain.data(), plain.size(), comp.data(), zn, ddict);
                bd.add(since(t));
                blobRaw += static_cast<double>(blob->size());
                blobZ += static_cast<double>(zn);
                if (round == 0) {
                    bool same = back.has_value();
                    if (same) {
                        same = server::world::region::chunkNbt::write(*back) ==
                               server::world::region::chunkNbt::write(*chunk);
                        for (size_t i = 0; same && i < chunk->sectionCount(); ++i) {
                            const auto& a = chunk->section(i);
                            const auto& b = back->section(i);
                            same = a.blockStateIds() == b.blockStateIds() && a.biomeIds() == b.biomeIds() &&
                                   a.nonAirBlockCount() == b.nonAirBlockCount() &&
                                   a.nonEmptyFluidCount() == b.nonEmptyFluidCount();
                        }
                    }
                    mism += same ? 0 : 1;
                    t = Clock::now();
                    auto viaNbt =
                        metamorph::region::chunkBlob::fromNbt(*root, server::world::region::chunkNbt::kDataVersion);
                    bn.add(since(t));
                    bool ok = viaNbt.has_value();
                    if (ok) {
                        t = Clock::now();
                        auto nbt2 = metamorph::region::chunkBlob::toNbt(*viaNbt);
                        bf.add(since(t));
                        auto c2 = cb::read(*viaNbt, server::world::kOverworldHeight);
                        ok = nbt2.has_value() && c2.has_value() &&
                             server::world::region::chunkNbt::write(*c2) ==
                                 server::world::region::chunkNbt::write(*chunk) &&
                             metamorph::region::chunkBlob::equivalent(
                                 nbt, wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(*nbt2)));
                    }
                    mismNbt += ok ? 0 : 1;
                }
            }
        }
        {
            std::vector<std::vector<std::uint8_t>> all;
            for (const auto& nbt : blobs) {
                all.push_back(metamorph::region::chunkBlob::encode(nbt, server::world::region::chunkNbt::kDataVersion));
            }
            std::vector<std::span<const std::uint8_t>> train(all.begin(), all.begin() + 192);
            auto bdict = metamorph::region::BswDictionary::train(train, 112640, 3);
            ZSTD_CDict* bc = ZSTD_createCDict(bdict->bytes().data(), bdict->bytes().size(), 3);
            double total = 0, micros = 0;
            for (const auto& b : all) {
                auto t0 = Clock::now();
                const size_t zz = ZSTD_compress_usingCDict(cctx, comp.data(), comp.size(), b.data(), b.size(), bc);
                micros += since(t0);
                total += static_cast<double>(zz);
            }
            std::printf("BLOB with a dictionary trained on blobs: zstd %.0f B per chunk, compress %.0f us\n",
                        total / static_cast<double>(all.size()), micros / static_cast<double>(all.size()));
        }
        const double m = static_cast<double>(blobs.size() * 3);
        std::printf("BLOB per chunk us: write %.0f  read %.0f  zstd-c %.0f  zstd-d %.0f | fromNbt %.0f toNbt %.0f\n",
                    bw.v / m, br.v / m, bz.v / m, bd.v / m, bn.v / static_cast<double>(blobs.size()),
                    bf.v / static_cast<double>(blobs.size()));
        std::printf("BLOB raw %.0f B, zstd %.0f B per chunk; chunk mismatches %.0f, nbt-path mismatches %.0f of %zu\n",
                    blobRaw / m, blobZ / m, mism, mismNbt, blobs.size());
    }
    malloc_trim(0);
    const size_t u0 = mallinfo2().uordblks;
    for (const auto& blob : blobs) {
        auto tag = wire::nbt::nbtIO::read(blob);
        auto chunk = server::world::region::chunkNbt::read(*tag->tag.get<wire::nbt::NbtCompound>(),
                                                           server::world::kOverworldHeight);
        keep.push_back(std::move(*chunk));
    }
    malloc_trim(0);
    const size_t u1 = mallinfo2().uordblks;
    size_t resident = 0;
    for (const auto& c : keep) {
        resident += c.residentBytes();
    }
    std::printf("resident per chunk: residentBytes() %.0f B, malloc delta %.0f B\n",
                static_cast<double>(resident) / static_cast<double>(keep.size()),
                static_cast<double>(u1 - u0) / static_cast<double>(keep.size()));
}

#include "tools/modelconvert/compile.hpp"
#include "tools/modelconvert/config.hpp"
#include "tools/modelconvert/mesh.hpp"
#include "tools/modelconvert/model.hpp"
#include "tools/util/expr.hpp"
#include "tools/util/expr_flag.hpp"
#include "tools/util/flag.hpp"
#include "tools/util/quote.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <err.h>
#include <utility>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fmt/core.h>

namespace modelconvert {
namespace {

class AxesFlag : public flag::FlagBase {
    Axes *m_ptr;

public:
    explicit AxesFlag(Axes *ptr) : m_ptr{ptr} {}

    flag::FlagArgument Argument() const override {
        return flag::FlagArgument::Required;
    }

    void Parse(std::optional<std::string_view> arg) override {
        assert(arg.has_value());
        try {
            *m_ptr = Axes::Parse(*arg);
        } catch (std::invalid_argument &ex) {
            std::string msg = fmt::format("invalid axes: {}", ex.what());
            throw flag::UsageError(msg);
        }
    }
};

// Wrapper for std::FILE.
class File {
    std::FILE *m_file;

public:
    File() : m_file{nullptr} {}
    File(const File &) = delete;
    File(File &&other) : m_file{other.m_file} { other.m_file = nullptr; }
    explicit File(std::FILE *file) : m_file{file} {}
    ~File() {
        if (m_file != nullptr)
            std::fclose(m_file);
    }
    File &operator=(const File &) = delete;
    File &operator=(File &&other) {
        std::swap(m_file, other.m_file);
        return *this;
    }
    operator std::FILE *() { return m_file; }
    operator bool() { return m_file != nullptr; }
    bool operator!() { return m_file == nullptr; }
};

[[noreturn]] void FailUsage(std::string_view msg) {
    fmt::print(stderr, "Error: {}\n", msg);
    std::exit(64);
}

struct Args {
    std::string model;
    std::string output;
    std::string output_stats;
    util::Expr::Ref meter;
    util::Expr::Ref scale;

    Config config;
};

void FixPath(std::string *path, std::string_view wd) {
    if (path->empty() || wd.empty() || (*path)[0] == '/') {
        return;
    }
    std::string result(wd);
    if (result[result.size() - 1] != '/') {
        result.push_back('/');
    }
    result.append(*path);
    *path = std::move(result);
}

Args ParseArgs(int argc, char **argv) {
    std::string wd;
    {
        const char *dir = std::getenv("BUILD_WORKSPACE_DIRECTORY");
        if (dir != nullptr) {
            wd.assign(dir);
        }
    }
    Args args{};
    args.config.texcoord_bits = 11;
    flag::Parser fl;
    fl.AddFlag(flag::String(&args.model), "model", "input model file", "FILE");
    fl.AddFlag(flag::String(&args.output), "output", "output data file",
               "FILE");
    fl.AddFlag(flag::String(&args.output_stats), "output-stats",
               "write human-readable model information to FILE", "FILE");
    fl.AddBoolFlag(&args.config.use_primitive_color, "use-primitive-color",
                   "use primitive color from material");
    fl.AddBoolFlag(&args.config.use_normals, "use-normals",
                   "use vertex normals");
    fl.AddBoolFlag(&args.config.use_vertex_colors, "use-vertex-colors",
                   "use vertex colors");
    fl.AddBoolFlag(&args.config.use_texcoords, "use-texcoords",
                   "use texture coordinates");
    fl.AddFlag(util::ExprFlag(&args.meter), "meter", "length of a meter",
               "EXPR");
    fl.AddFlag(util::ExprFlag(&args.scale), "scale", "amount to scale model",
               "EXPR");
    fl.AddFlag(flag::Int(&args.config.texcoord_bits), "texcoord-bits",
               "fractional bits of precision for texture coordinates");
    fl.AddFlag(AxesFlag(&args.config.axes), "axes",
               "remap axes, default 'x,y,z'", "AXES");
    fl.AddBoolFlag(&args.config.animate, "animate", "convert animations");
    flag::ProgramArguments prog_args{argc - 1, argv + 1};
    try {
        fl.ParseAll(prog_args);
    } catch (flag::UsageError &ex) { FailUsage(ex.what()); }
    if (args.model.empty()) {
        FailUsage("missing required flag -model");
    }
    FixPath(&args.model, wd);
    FixPath(&args.output, wd);
    FixPath(&args.output_stats, wd);
    if (!args.scale) {
        FailUsage("missing required flag -scale");
    }
    return args;
}

void WriteFile(const std::string &out, const std::vector<uint8_t> &data) {
    FILE *fp = std::fopen(out.c_str(), "wb");
    if (fp == nullptr) {
        err(1, "could not open %s", out.c_str());
    }
    size_t n = fwrite(data.data(), 1, data.size(), fp);
    if (n != data.size()) {
        err(1, "could not write %s", out.c_str());
    }
    int r = fclose(fp);
    if (r != 0) {
        err(1, "could not write %s", out.c_str());
    }
}

void Main(int argc, char **argv) {
    Args args = ParseArgs(argc, argv);
    Config cfg = args.config;
    Assimp::Importer importer;

    {
        util::Expr::Env env;
        if (args.meter) {
            double meter = args.meter->Eval(env);
            env.emplace("meter", meter);
        }
        double scale = args.scale->Eval(env);
        if (!std::isfinite(scale) || scale <= 0) {
            fmt::print(stderr, "Error: scale must be a positive number\n");
            std::exit(1);
        }
        cfg.scale = scale;
    }

    File stats;
    if (!args.output_stats.empty()) {
        stats = File{std::fopen(args.output_stats.c_str(), "w")};
        if (!stats) {
            err(1, "could not open '%s'", args.output_stats.c_str());
        }
        fmt::print(stats, "Config:\n");
        fmt::print(stats, "    Primitive color: {}\n", cfg.use_primitive_color);
        fmt::print(stats, "    Normals: {}\n", cfg.use_normals);
        fmt::print(stats, "    Texcoords: {}\n", cfg.use_texcoords);
        fmt::print(stats, "    Vertex colors: {}\n", cfg.use_vertex_colors);
        fmt::print(stats, "    Texcoord bits: {}\n", cfg.texcoord_bits);
        fmt::print(stats, "    Scale: {}\n", cfg.scale);
        fmt::print(stats, "    Axes: {}\n", cfg.axes.ToString());
        fmt::print(stats, "    Animate: {}\n", cfg.animate);
        fmt::print(stats, "\n");
    }

    // Import mesh.
    const aiScene *scene = importer.ReadFile(
        args.model, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
    if (scene == nullptr) {
        fmt::print(stderr, "Error: could not import {}: {}\n",
                   util::Quote(args.model), importer.GetErrorString());
        std::exit(1);
    }

    Mesh mesh = Mesh::Import(cfg, stats, scene);

    gbi::Model model = gbi::CompileMesh(mesh, cfg, stats);
    if (stats) {
        fmt::print(stats, "Display list commands: {}\n", model.command.size());
        fmt::print(stats, "Vertexes: {}\n", model.vertex.size());
        fmt::print(stats, "Animations: {}\n", model.animation.size());
        fmt::print(stats, "Frames: {}\n", model.frame.size());
    }
    if (!args.output.empty()) {
        std::vector<uint8_t> data = model.Emit(cfg);
        WriteFile(args.output, data);
    }
}

} // namespace
} // namespace modelconvert

int main(int argc, char **argv) {
    modelconvert::Main(argc, argv);
    return 0;
}

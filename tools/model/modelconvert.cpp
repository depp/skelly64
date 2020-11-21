#include "tools/modelconvert/mesh.hpp"
#include "tools/util/flag.hpp"
#include "tools/util/quote.hpp"

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
    fl.AddFlag(flag::Float32(&args.config.scale), "scale",
               "amount to scale model", "N");
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
    if (!std::isfinite(args.config.scale) || args.config.scale <= 0.0) {
        FailUsage("-scale must be a positive number");
    }
    return args;
}

const char spaces[81] =
    "                                        "
    "                                        ";

void Indent(std::FILE *fp, int n) {
    while (n > 80) {
        std::fwrite(spaces, 1, 80, fp);
    }
    if (n > 0) {
        std::fwrite(spaces, 1, n, fp);
    }
}

void VisitNode(std::FILE *stats, aiNode *node, int indent = 2) {
    Indent(stats, indent);
    fmt::print(stats, "{} meshes={}\n", util::Quote(Str(node->mName)),
               node->mNumMeshes);
    for (aiNode **ptr = node->mChildren, **end = ptr + node->mNumChildren;
         ptr != end; ptr++) {
        VisitNode(stats, *ptr, indent + 2);
    }
}

void MeshInfo(std::FILE *stats, const Mesh &mesh) {
    fmt::print(stats, "Vertexes: {}\nTriangles: {}\n",
               mesh.vertexes.vertexes.size(), mesh.triangles.size());
    const std::array<int16_t, 3> min = mesh.bounds_min, max = mesh.bounds_max;
    fmt::print(stats, "Bounds: ({}, {}, {}) ({}, {}, {})\n", min[0], min[1],
               min[2], max[0], max[1], max[2]);
    fmt::print(stats, "Size: ({}, {}, {})\n", max[0] - min[0], max[1] - min[1],
               max[2] - min[2]);
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
    Assimp::Importer importer;

    File stats;
    if (!args.output_stats.empty()) {
        stats = File{std::fopen(args.output_stats.c_str(), "w")};
        if (!stats) {
            err(1, "could not open '%s'", args.output_stats.c_str());
        }
    }

    const aiScene *scene = importer.ReadFile(
        args.model, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
    if (scene == nullptr) {
        fmt::print(stderr, "Error: could not import {}: {}",
                   util::Quote(args.model), importer.GetErrorString());
        std::exit(1);
    }
    if (stats) {
        fmt::print(stats, "Nodes:\n");
        VisitNode(stats, scene->mRootNode);
    }
    VertexSet verts;

    Mesh mesh{};
    for (aiMesh **ptr = scene->mMeshes,
                **end = scene->mMeshes + scene->mNumMeshes;
         ptr != end; ptr++) {
        mesh.AddMesh(args.config, stats, *ptr);
    }
    if (stats) {
        MeshInfo(stats, mesh);
    }
    BatchMesh bmesh = mesh.MakeBatches(32);
    if (stats) {
        bmesh.Dump(stats);
    }
    std::vector<Material> materials;
    for (aiMaterial **ptr = scene->mMaterials,
                    **end = ptr + scene->mNumMaterials;
         ptr != end; ptr++) {
        materials.push_back(Material::Import(args.config, *ptr));
    }
    if (!args.output.empty()) {
        std::vector<uint8_t> data = bmesh.EmitGBI(args.config, materials);
        WriteFile(args.output, data);
    }
}

} // namespace
} // namespace modelconvert

int main(int argc, char **argv) {
    modelconvert::Main(argc, argv);
    return 0;
}

#include "tools/modelconvert/mesh.hpp"
#include "tools/util/flag.hpp"
#include "tools/util/quote.hpp"

#include <cmath>
#include <cstdlib>
#include <err.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fmt/core.h>

namespace modelconvert {
namespace {

[[noreturn]] void FailUsage(std::string_view msg) {
    fmt::print(stderr, "Error: {}\n", msg);
    std::exit(64);
}

struct Args {
    std::string model;
    std::string output;
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
    Args args;
    flag::Parser fl;
    fl.AddFlag(flag::String(&args.model), "model", "input model file", "FILE");
    fl.AddFlag(flag::String(&args.output), "output", "output data file",
               "FILE");
    flag::ProgramArguments prog_args{argc - 1, argv + 1};
    try {
        fl.ParseAll(prog_args);
    } catch (flag::UsageError &ex) { FailUsage(ex.what()); }
    if (args.model.empty()) {
        FailUsage("missing required flag -model");
    }
    FixPath(&args.model, wd);
    FixPath(&args.output, wd);
    return args;
}

const char spaces[81] =
    "                                        "
    "                                        ";

void Indent(int n) {
    while (n > 80) {
        std::fwrite(spaces, 1, 80, stdout);
    }
    if (n > 0) {
        std::fwrite(spaces, 1, n, stdout);
    }
}

void VisitNode(aiNode *node, int indent = 0) {
    Indent(indent);
    fmt::print(stdout, "{} meshes={}\n", util::Quote(Str(node->mName)),
               node->mNumMeshes);
    for (aiNode **ptr = node->mChildren, **end = ptr + node->mNumChildren;
         ptr != end; ptr++) {
        VisitNode(*ptr, indent + 2);
    }
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

    const aiScene *scene = importer.ReadFile(
        args.model, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
    if (scene == nullptr) {
        fmt::print(stderr, "Error: could not import {}: {}",
                   util::Quote(args.model), importer.GetErrorString());
        std::exit(1);
    }
    if (args.output.empty()) {
        VisitNode(scene->mRootNode);
    }
    VertexSet verts;

    Mesh mesh;
    for (aiMesh **ptr = scene->mMeshes,
                **end = scene->mMeshes + scene->mNumMeshes;
         ptr != end; ptr++) {
        mesh.AddMesh(*ptr);
    }
    BatchMesh bmesh = mesh.MakeBatches(32);
    if (args.output.empty()) {
        bmesh.Dump();
    }
    std::vector<Material> materials;
    for (aiMaterial **ptr = scene->mMaterials,
                    **end = ptr + scene->mNumMaterials;
         ptr != end; ptr++) {
        materials.push_back(Material::Import(*ptr));
    }
    if (!args.output.empty()) {
        std::vector<uint8_t> data = bmesh.EmitGBI(materials);
        WriteFile(args.output, data);
    }
}

} // namespace
} // namespace modelconvert

int main(int argc, char **argv) {
    modelconvert::Main(argc, argv);
    return 0;
}

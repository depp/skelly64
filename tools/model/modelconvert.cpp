#include "tools/util/flag.hpp"
#include "tools/util/quote.hpp"

#include <cstdlib>
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace modelconvert {
namespace {

[[noreturn]] void FailUsage(std::string_view msg) {
    std::cerr << "Error: " << msg << '\n';
    std::exit(64);
}

struct Args {
    std::string model;
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
    flag::ProgramArguments prog_args{argc - 1, argv + 1};
    try {
        fl.ParseAll(prog_args);
    } catch (flag::UsageError &ex) { FailUsage(ex.what()); }
    if (args.model.empty()) {
        FailUsage("missing required flag -model");
    }
    FixPath(&args.model, wd);
    return args;
}

std::string_view Str(const aiString &s) {
    return std::string_view(s.data, s.length);
}

const char spaces[81] =
    "                                        "
    "                                        ";

void Indent(int n) {
    while (n > 80) {
        std::cout << std::string_view(spaces, 80);
        n -= 80;
    }
    if (n > 0) {
        std::cout << std::string_view(spaces, n);
    }
}

void VisitNode(aiNode *node, int indent = 0) {
    Indent(indent);
    std::cout << util::Quote(Str(node->mName)) << '\n';
    for (aiNode **ptr = node->mChildren, **end = ptr + node->mNumChildren;
         ptr != end; ptr++) {
        VisitNode(*ptr, indent + 2);
    }
}

void Main(int argc, char **argv) {
    Args args = ParseArgs(argc, argv);
    Assimp::Importer importer;

    const aiScene *scene = importer.ReadFile(
        args.model, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
    if (scene == nullptr) {
        std::cout << "Error: could not import " << util::Quote(args.model)
                  << ": " << importer.GetErrorString() << '\n';
        std::exit(1);
    }
    VisitNode(scene->mRootNode);
    (void)&args;
}

} // namespace
} // namespace modelconvert

int main(int argc, char **argv) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    modelconvert::Main(argc, argv);
    return 0;
}

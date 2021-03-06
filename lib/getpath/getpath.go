package getpath

import (
	"os"
	"path/filepath"
)

var (
	wd       string
	runfiles string
)

func init() {
	wd = os.Getenv("BUILD_WORKING_DIRECTORY")
	if wd == "" {
		// This is a cheesy way to get the runfiles path, but it works.
		runfiles = filepath.Join(os.Args[0]+".runfiles", "com_github_depp_skelly64")
	}
}

// GetPath returns the path to the file, which will be correct even when run
// from Bazel.
func GetPath(filename string) string {
	if filename != "" && wd != "" && !filepath.IsAbs(filename) {
		return filepath.Join(wd, filename)
	}
	return filename
}

// GetTool returns the pat to a tool. The tool must be listed in the data
// dependencies for the running executable, and the filename should be the
// package and target name. This will work with "bazel run", genrules, and
// Starlark actions.
//
// See: https://docs.bazel.build/versions/master/skylark/rules.html#runfiles
func GetTool(filename string) string {
	return filepath.Join(runfiles, filename)
}

// Relative returns the relative path from base to target, if target is a
// descendant of base. Otherwise, returns the empty string.
func Relative(base, target string) string {
	if len(target) >= len(base) && target[:len(base)] == base {
		if len(target) == len(base) {
			return "."
		}
		if target[len(base)] == byte(filepath.Separator) {
			rel := target[len(base)+1:]
			if rel == "" {
				rel = "."
			}
			return rel
		}
	}
	return ""
}

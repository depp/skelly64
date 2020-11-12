package getpath

import (
	"os"
	"path/filepath"
)

var wd string

func init() {
	wd = os.Getenv("BUILD_WORKSPACE_DIRECTORY")
}

// GetPath returns the path to the file, which will be correct even when run
// from Bazel.
func GetPath(filename string) string {
	if filename != "" && wd != "" && !filepath.IsAbs(filename) {
		return filepath.Join(wd, filename)
	}
	return filename
}

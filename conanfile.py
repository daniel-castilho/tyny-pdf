"""Root Conan recipe (docs/dependency-policy.md section 2, ADR-0004): the graph the
shipped binaries are built against, test-only dependencies included. Until story 1.4
consumes GoogleTest the lockfile is the deliverable - CI and every contributor resolve
the identical graph from conan.lock instead of hoping ConanCenter looks the same today
and tomorrow.
"""

from conan import ConanFile


class TynyPdfConan(ConanFile):
    name = "tynypdf"
    version = "0.1.0"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.test_requires("gtest/1.17.0")

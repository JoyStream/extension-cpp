from conans import ConanFile, CMake
import os

class ExtensionTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = "Extension/0.0.0@%s/%s" % ("joystream", "testing")
    generators = "cmake"

    def build(self):
        cmake = CMake(self.settings)
        self.run('cmake "%s" %s' % (self.conanfile_directory, cmake.command_line))
        self.run("cmake --build . %s" % cmake.build_config)

    def imports(self):
        self.copy("*.dll", src="lib", dst="bin")
        self.copy("*.dylib", src="lib", dst="bin")

    def test(self):
        os.chdir("bin")
        self.run(".%sexample" % os.sep)

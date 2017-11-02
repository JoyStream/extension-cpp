from conans import ConanFile, CMake, tools
import os

class ExtensionBase(ConanFile):
    name = "Extension"
    version = "0.1.5"
    license = "(c) JoyStream Inc. 2016-2017"
    url = "https://github.com/JoyStream/extension-conan.git"
    git_repo = "git@github.com:JoyStream/extension-cpp.git"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"
    requires = "ProtocolSession/0.1.1@joystream/stable", "Libtorrent/1.1.1@joystream/stable"
    build_policy = "missing"

    def source(self):
        raise Error("")

    def configure(self):
        self.options["Libtorrent"].deprecated_functions=False

    def build(self):
        cmake = CMake(self.settings)
        self.run('cmake repo/sources %s' % (cmake.command_line))
        self.run("cmake --build . %s" % cmake.build_config)

    def package(self):
        self.copy("*.hpp", dst="include", src="repo/sources/include/")
        self.copy("*.a", dst="lib", keep_path=False)
        self.copy("*.lib", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["extension"]

        if str(self.settings.compiler) != "Visual Studio":
            self.cpp_info.cppflags.append("-std=c++11")

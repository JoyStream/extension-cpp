from conans import ConanFile, CMake, tools
import os

class ExtensionBase(ConanFile):
    name = "Extension"
    version = "0.3.1"
    license = "(c) JoyStream Inc. 2016-2018"
    url = "https://github.com/JoyStream/extension-cpp.git"
    repo_ssh_url = "git@github.com:JoyStream/extension-cpp.git"
    repo_https_url = "https://github.com/JoyStream/extension-cpp.git"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"
    requires = "ProtocolSession/0.3.0@joystream/stable", "Libtorrent/1.1.1@joystream/stable", "Common/0.2.0@joystream/stable"
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

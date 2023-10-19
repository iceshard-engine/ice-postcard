from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps

class IceShardPostcardRecipe(ConanFile):
    name = "ice-postcard"
    version = "0.1.0"
    package_type = "library"
    user = "iceshard"
    channel = "stable"

    # Optional metadata
    license = "MIT"
    author = "dandielo@iceshard.net"
    url = "https://github.com/iceshard-engine/postcard"
    description = "Very small library hiding data in LSB of RGB channels of an image."
    topics = ("image-processing", "data-storage", "image", "data", "data-hiding", "iceshard")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True,False], "fPIC": [True, False]}
    default_options = {"shared":False,"fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "private/*", "public/*"

    tool_requires = "cmake/[>=3.25.3 <4.0]", "ninja/[>=1.11.1 <2.0]"

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        self.settings.compiler.cppstd = 20

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, "Ninja")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["postcard"]

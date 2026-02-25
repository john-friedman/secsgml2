import sys
from setuptools import Extension, setup
from Cython.Build import cythonize

# Build compiler args per platform
def get_compile_args():
    if sys.platform == "win32":
        # MSVC flags
        return ["/O2", "/W3"]
    else:
        # gcc/clang flags
        args = ["-O3", "-pipe"]
        # Only add -march=native for non-cross-compile situations.
        # On Linux aarch64 under QEMU, native detection is unreliable,
        # so cibuildwheel sets CIBW_ARCHS and we can check the target.
        # Simplest: just omit -march=native entirely and let the ifdefs
        # in your C code handle SIMD based on what the compiler detects.
        return args


ext_modules = [
    Extension(
        "secsgml2._core",
        sources=[
            "secsgml2/_core.pyx",
            "c/secsgmlc/src/secsgml.c",
            "c/secsgmlc/src/standardize_submission_metadata.c",
            "c/secsgmlc/src/uudecode.c",
        ],
        include_dirs=["c/secsgmlc/src"],
        extra_compile_args=get_compile_args(),
    ),
    Extension(
        "secsgml2.uu",
        sources=[
            "secsgml2/uu.pyx",
            "c/secsgmlc/src/uudecode.c",
        ],
        include_dirs=["c/secsgmlc/src"],
        extra_compile_args=get_compile_args(),
    ),
]


setup(
    packages=["secsgml2"],
    ext_modules=cythonize(
        ext_modules,
        language_level="3",
        compiler_directives={"boundscheck": False, "wraparound": False},
    ),
)

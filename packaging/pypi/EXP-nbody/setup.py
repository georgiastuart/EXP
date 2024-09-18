from skbuild import setup

version="7.7.99"

# Setup for the CopyAndSdist subclass

import os
import pathlib
import shutil

from setuptools.command.sdist import sdist

class CopyAndSdist(sdist):
  file_list = []
  directory_list = []
  def run(self):
    self.copy_src()
    super().run()
    self.clean_src()

  @classmethod
  def clean_src(cls):
    print(f"Cleaning the packaging directory, {os.getcwd()}...")
    for file in cls.file_list:
      try:
        os.remove(file)
      except (FileNotFoundError, TypeError):
        try:
          os.remove(file[1])
        except FileNotFoundError:
          print(f"File {file} did not exist")
      print(f"Removed file {file}")

    for directory in cls.directory_list:
      try:
        shutil.rmtree(directory, ignore_errors=True)
      except TypeError:
        shutil.rmtree(directory[1], ignore_errors=True)
        print(f"Removed directory {directory}")

  @classmethod
  def copy_src(cls):
    # Set up the necessary source files
    rootdir = pathlib.Path.cwd().joinpath("../../..").resolve()

    for directory in cls.directory_list:
      if isinstance(directory, list):
        src = directory[0]
        dest = directory[1]
      else:
        src = directory
        dest = directory
      shutil.copytree(rootdir.joinpath(src), pathlib.Path.cwd().joinpath(dest), dirs_exist_ok=True)

    for file in cls.file_list:
      if isinstance(file, list):
        src = file[0]
        dest = file[1]
      else:
        src = file
        dest = file
      shutil.copy(rootdir.joinpath(src), pathlib.Path.cwd().joinpath(dest))


CopyAndSdist.file_list = [
  'AUTHORS',
  'ChangeLog',
  'CMakeLists.txt',
  'config_cmake.h_in',
  'COPYING',
  'INSTALL',
  'INSTALL.md',
  'LICENSE',
  'NEWS',
  'README.md',
]

# Directories
CopyAndSdist.directory_list = [
  'cmake',
  'src',
]

# Install specifications

setup(
    name="EXP-nbody",
    version=version,
    description="Nbody EXPansion Code - Nbody",
    author="",
    license="GPL-3.0",
    packages=["EXP-nbody"],
    python_requires=">=3.8",
    install_requires=[
      'exp-libraries'
    ],
    package_dir={
      "EXP-nbody": "src"
    },
    cmdclass={
      "sdist": CopyAndSdist,
    },
    cmake_install_dir="",
    cmake_minimum_required_version="3.21",
    cmake_languages=("C", "CXX", "Fortran"),
    cmake_args=[
      "-DCMAKE_CXX_STANDARD=17",
      "-DINSTALL_HEADERS=OFF",
      "-DBUILD_COMMON_LIBRARIES=OFF",
      "-DINSTALL_CMAKE_FIND=ON",
      "-DENABLE_NBODY=ON",
      "-DENABLE_PYEXP=OFF",
      "-DBUILD_UTILS=OFF",
      "-DUSE_SUBMODULES=OFF",
      "-DENABLE_PNG=OFF", # png++ is only compatible with libpng 1.2.X - defunct      "-DENABLE_USER=OFF",
      "-DENABLE_USER=OFF",
      "-DENABLE_TESTS=OFF",
      "-DENABLE_XDR=ON",
      "-DENABLE_DSMC=OFF", # missing from extern so need clarification on what dependency it needs
      "-DCMAKE_BUILD_TYPE=RELEASE",
      "-Wno-dev"
    ]
    # extras_require={
    #   "cuda": []
    # }
)


#!python

# Copyright (c) 2016 Ryan Prichard
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

#
# Run with native CPython 2.7.
#
# These programs must be in your Path:
#  - 7z.exe
#  - git.exe
#
# This script looks for MSVC using a version-specific environment variable,
# such as VS140COMNTOOLS for MSVC 2015.
#

import common_ship

import argparse
import os
import shutil
import subprocess
import sys

os.chdir(common_ship.topDir)
ZIP_TOOL = common_ship.requireExe("7z.exe")

MSVC_VERSION_TABLE = {
    "2015" : {
        "package_name" : "msvc2015",
        "gyp_version" : "2015",
        "common_tools_env" : "VS140COMNTOOLS",
        "xp_toolset" : "v140_xp",
    },
    "2013" : {
        "package_name" : "msvc2013",
        "gyp_version" : "2013",
        "common_tools_env" : "VS120COMNTOOLS",
        "xp_toolset" : "v120_xp",
    },
}

ARCH_TABLE = {
    "x64" : {
        "msvc_platform" : "x64",
    },
    "ia32" : {
        "msvc_platform" : "Win32",
    },
}

def readArguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--msvc-version", default="2015")
    ret = parser.parse_args()
    if ret.msvc_version not in MSVC_VERSION_TABLE:
        sys.exit("Error: unrecognized version: " + ret.msvc_version + ". " +
            "Versions: " + " ".join(sorted(MSVC_VERSION_TABLE.keys())))
    return ret

ARGS = readArguments()

def checkoutGyp():
    if os.path.isdir("build-gyp"):
        return
    subprocess.check_call([
        "git.exe",
        "clone",
        "https://chromium.googlesource.com/external/gyp",
        "build-gyp"
    ])

def cleanMsvc():
    common_ship.rmrf("""
        src/Release src/.vs
        src/*.vcxproj src/*.vcxproj.filters src/*.sln src/*.sdf
    """.split())

def build(arch, packageDir, xp=False):
    archInfo = ARCH_TABLE[arch]
    versionInfo = MSVC_VERSION_TABLE[ARGS.msvc_version]

    subprocess.check_call([
        sys.executable,
        "../build-gyp/gyp_main.py",
        "winpty.gyp",
        "-I", "configurations.gypi",
        "-G", "msvs_version=" + versionInfo["gyp_version"]] +
        (["-D", "WINPTY_MSBUILD_TOOLSET=" + versionInfo["xp_toolset"]] if xp else []),
        cwd="src")
    devCmdPath = os.path.join(os.environ[versionInfo["common_tools_env"]], "VsDevCmd.bat")
    if not os.path.isfile(devCmdPath):
        sys.exit("Error: MSVC environment script missing: " + devCmdPath)
    subprocess.check_call(
        '"' + devCmdPath + '" && ' +
        "msbuild winpty.sln /m /p:Platform=" + ARCH_TABLE[arch]["msvc_platform"],
        shell=True,
        cwd="src")

    archPackageDir = os.path.join(packageDir, arch)
    if xp:
        archPackageDir += "_xp"

    common_ship.mkdir(archPackageDir + "/bin")
    common_ship.mkdir(archPackageDir + "/lib")

    binSrc = os.path.join(common_ship.topDir, "src/Release", archInfo["msvc_platform"])

    shutil.copy(binSrc + "/winpty.dll",                 archPackageDir + "/bin")
    shutil.copy(binSrc + "/winpty-agent.exe",           archPackageDir + "/bin")
    shutil.copy(binSrc + "/winpty-debugserver.exe",     archPackageDir + "/bin")
    shutil.copy(binSrc + "/winpty.lib",                 archPackageDir + "/lib")

def buildPackage():
    versionInfo = MSVC_VERSION_TABLE[ARGS.msvc_version]

    packageName = "winpty-%s-%s" % (
        common_ship.winptyVersion,
        versionInfo["package_name"],
    )

    packageRoot = os.path.join(common_ship.topDir, "ship/packages")
    packageDir = os.path.join(packageRoot, packageName)
    packageFile = packageDir + ".zip"

    common_ship.rmrf([packageDir])
    common_ship.rmrf([packageFile])
    common_ship.mkdir(packageDir)

    checkoutGyp()
    cleanMsvc()
    build("ia32", packageDir, True)
    build("x64", packageDir, True)
    cleanMsvc()
    build("ia32", packageDir)
    build("x64", packageDir)

    topDir = common_ship.topDir

    common_ship.mkdir(packageDir + "/include")
    shutil.copy(topDir + "/src/include/winpty.h",               packageDir + "/include")
    shutil.copy(topDir + "/src/include/winpty_constants.h",     packageDir + "/include")
    shutil.copy(topDir + "/LICENSE",                            packageDir)
    shutil.copy(topDir + "/README.md",                          packageDir)
    shutil.copy(topDir + "/RELEASES.md",                        packageDir)

    subprocess.check_call([ZIP_TOOL, "a", packageFile, "."], cwd=packageDir)

    common_ship.rmrf([packageDir])

if __name__ == "__main__":
    buildPackage()

module Main where

import Dib
import Dib.Builders.C
import Dib.Target
import qualified Data.Text as T

liblaminaFSInfo = defaultGCCConfig {
  outputName = "liblaminaFS.so",
  targetName = "laminaFS",
  srcDir = "src",
  commonCompileFlags = "-Wall -Werror -fPIC",
  cCompileFlags = "",
  cxxCompileFlags = "--std=c++11",
  linkFlags = "-shared",
  outputLocation = ObjAndBinDirs "obj" ".",
  includeDirs = ["src"]
}

liblaminaFS = makeCTarget liblaminaFSInfo
cleanLamina = makeCleanTarget liblaminaFSInfo

testsInfo = defaultGCCConfig {
  outputName = "test",
  targetName = "tests",
  srcDir = "tests",
  commonCompileFlags = "",
  cCompileFlags = "",
  cxxCompileFlags = "--std=c++11",
  linkFlags = "-L. -llaminaFS -lstdc++",
  extraLinkDeps = ["liblaminaFS.so"],
  outputLocation = ObjAndBinDirs "obj" ".",
  includeDirs = ["src", "tests"]
}

tests = addDependency (makeCTarget testsInfo) liblaminaFS
cleanTests = makeCleanTarget testsInfo

allTarget = makePhonyTarget "all" [liblaminaFS, tests]

targets = [allTarget, liblaminaFS, cleanLamina, tests, cleanTests]

main = dib targets

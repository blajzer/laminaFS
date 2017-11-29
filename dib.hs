module Main where

import Dib
import Dib.Builders.C
import Dib.Target
import qualified Data.Text as T

exeExt platform = if platform == "mingw32" then ".exe" else ""
soExt platform = if platform == "mingw32" then ".dll" else ".so"

mingw32Config = defaultGXXConfig {
  compiler = "x86_64-w64-mingw32-gcc",
  linker = "x86_64-w64-mingw32-g++ ",
  archiver = "i86_64-w64-mingw32-ar"
}

liblaminaFSInfo platform = (getCompiler platform) {
  outputName = "liblaminaFS" `T.append` (soExt platform),
  targetName = "laminaFS-" `T.append` platform,
  srcDir = "src",
  commonCompileFlags = "-g -Wall -Werror -fPIC -O2",
  cCompileFlags = "--std=c11",
  cxxCompileFlags = "--std=c++14",
  linkFlags = "-shared -lpthread",
  outputLocation = ObjAndBinDirs ("obj/" `T.append` platform) ".",
  includeDirs = ["src"]
}

liblaminaFS platform = makeCTarget $ liblaminaFSInfo platform
cleanLamina platform = makeCleanTarget $ liblaminaFSInfo platform

testsInfo platform = (getCompiler platform) {
  outputName = "test" `T.append` (exeExt platform),
  targetName = "tests-" `T.append` platform,
  srcDir = "tests",
  commonCompileFlags = "-g -O2",
  cCompileFlags = "--std=c11",
  cxxCompileFlags = "--std=c++14",
  linkFlags = "-L. -llaminaFS -lstdc++ -lpthread",
  extraLinkDeps = ["liblaminaFS.so"],
  outputLocation = ObjAndBinDirs ("obj/" `T.append` platform) ".",
  includeDirs = ["src", "tests"]
}

tests platform = addDependency (makeCTarget $ testsInfo platform) $ liblaminaFS platform
cleanTests platform = makeCleanTarget $ testsInfo platform

allTarget platform = makePhonyTarget "all" [liblaminaFS platform, tests platform]

targets platform = [allTarget platform,  liblaminaFS platform, cleanLamina platform, tests platform, cleanTests platform]

getBuildPlatform = makeArgDictLookupFunc "PLATFORM" "local"
getCompiler platform = if platform == "mingw32" then mingw32Config else defaultGXXConfig

main = do
  argDict <- getArgDict
  dib $ targets $ T.pack $ getBuildPlatform argDict

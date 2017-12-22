module Main where

import Dib
import Dib.Builders.C
import Dib.Target
import qualified Data.Text as T

exeExt platform = if platform == "mingw32" then ".exe" else ""
soExt platform = if platform == "mingw32" then ".dll" else ".so"

sanitizerFlags "undefined" = " -fsanitize=undefined"
sanitizerFlags "address" = " -fsanitize=address"
sanitizerFlags "thread" = " -fsanitize=thread"
sanitizerFlags "" = ""
sanitizerFlags s@_ = error $ "Unknown sanitizer: " ++ s

mingw32Config = defaultGXXConfig {
  compiler = "x86_64-w64-mingw32-gcc",
  linker = "x86_64-w64-mingw32-g++ ",
  archiver = "i86_64-w64-mingw32-ar"
}

liblaminaFSInfo platform sanitizer = (getCompiler platform) {
  outputName = "liblaminaFS" `T.append` (soExt platform),
  targetName = "laminaFS-" `T.append` platform,
  srcDir = "src",
  commonCompileFlags = "-g -Wall -Werror -fPIC -O2" `T.append` (sanitizerFlags sanitizer),
  cCompileFlags = "--std=c11",
  cxxCompileFlags = "--std=c++14",
  linkFlags = "-shared -lpthread" `T.append` (sanitizerFlags sanitizer),
  outputLocation = ObjAndBinDirs ("obj/" `T.append` platform) ".",
  includeDirs = ["src"]
}

liblaminaFS platform sanitizer = makeCTarget $ liblaminaFSInfo platform sanitizer
cleanLamina platform sanitizer = makeCleanTarget $ liblaminaFSInfo platform sanitizer

testsInfo platform sanitizer = (getCompiler platform) {
  outputName = "test" `T.append` (exeExt platform),
  targetName = "tests-" `T.append` platform,
  srcDir = "tests",
  commonCompileFlags = "-g -O2" `T.append` (sanitizerFlags sanitizer),
  cCompileFlags = "--std=c11",
  cxxCompileFlags = "--std=c++14",
  linkFlags = "-L. -llaminaFS -lstdc++ -lpthread" `T.append` (sanitizerFlags sanitizer),
  extraLinkDeps = ["liblaminaFS.so"],
  outputLocation = ObjAndBinDirs ("obj/" `T.append` platform) ".",
  includeDirs = ["src", "tests"]
}

tests platform sanitizer = addDependency (makeCTarget $ testsInfo platform sanitizer) $ liblaminaFS platform sanitizer
cleanTests platform sanitizer = makeCleanTarget $ testsInfo platform sanitizer

allTarget platform sanitizer = makePhonyTarget "all" [liblaminaFS platform sanitizer, tests platform sanitizer]

targets platform sanitizer = [allTarget platform sanitizer,  liblaminaFS platform sanitizer, cleanLamina platform sanitizer, tests platform sanitizer, cleanTests platform sanitizer]

getBuildPlatform = makeArgDictLookupFunc "PLATFORM" "local"
getSanitizer = makeArgDictLookupFunc "SANITIZER" ""
getCompiler platform = if platform == "mingw32" then mingw32Config else defaultGXXConfig

main = do
  argDict <- getArgDict
  let platform = T.pack $ getBuildPlatform argDict
  let sanitizer = getSanitizer argDict
  dib $ targets platform sanitizer

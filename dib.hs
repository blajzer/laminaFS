module Main where

import Dib
import Dib.Builders.C
import Dib.Target
import Data.Monoid
import qualified Data.Text as T

data Configuration = Configuration {
  platform :: T.Text,
  buildType :: T.Text,
  exeExt :: T.Text,
  soExt :: T.Text,
  sanitizerFlags :: T.Text,
  buildFlags :: T.Text
}

mingw32Config = defaultGXXConfig {
  compiler = "x86_64-w64-mingw32-gcc",
  linker = "x86_64-w64-mingw32-g++ ",
  archiver = "i86_64-w64-mingw32-ar"
}

-- liblaminaFS targets
liblaminaFSInfo config = (getCompiler $ platform config) {
  outputName = "liblaminaFS" <> soExt config,
  targetName = "laminaFS-" <> platform config <> "-" <> buildType config,
  srcDir = "src",
  commonCompileFlags = "-Wall -Wextra -Werror -fPIC " <> buildFlags config <> sanitizerFlags config,
  cCompileFlags = "--std=c11",
  cxxCompileFlags = "--std=c++14",
  linkFlags = "-shared -lpthread" <> sanitizerFlags config,
  outputLocation = ObjAndBinDirs ("obj/" <> platform config) ("lib/" <> platform config <> "-" <> buildType config),
  includeDirs = ["src"]
}

liblaminaFS config = makeCTarget $ liblaminaFSInfo config
cleanLamina config = makeCleanTarget $ liblaminaFSInfo config

-- Test targets
testsInfo config = (getCompiler $ platform config) {
  outputName = "test" <> exeExt config,
  targetName = "tests-" <> platform config <> "-" <> buildType config,
  srcDir = "tests",
  commonCompileFlags = "-Wall -Wextra -Werror " <> buildFlags config <> sanitizerFlags config,
  cCompileFlags = "--std=c11",
  cxxCompileFlags = "--std=c++14",
  linkFlags = "-L./lib/" <> platform config <> "-" <> buildType config <> " -llaminaFS -lstdc++ -lpthread" <> sanitizerFlags config,
  extraLinkDeps = ["liblaminaFS.so"],
  outputLocation = ObjAndBinDirs ("obj/" <> platform config) ("bin/" <> platform config <> "-" <> buildType config),
  includeDirs = ["src", "tests"]
}

tests config = addDependency (makeCTarget $ testsInfo config) $ liblaminaFS config
cleanTests config = makeCleanTarget $ testsInfo config

-- Targets
allTarget config = makePhonyTarget "all" [liblaminaFS config, tests config]
targets config = [allTarget config,  liblaminaFS config, cleanLamina config, tests config, cleanTests config]

-- Configuration related functions
getBuildPlatform = makeArgDictLookupFunc "PLATFORM" "local"
getSanitizer = makeArgDictLookupFunc "SANITIZER" ""
getBuildType = makeArgDictLookupFunc "BUILD" "release"
getCompiler platform = if platform == "mingw32" then mingw32Config else defaultGXXConfig

sanitizerToFlags "undefined" = " -fsanitize=undefined"
sanitizerToFlags "address" = " -fsanitize=address"
sanitizerToFlags "thread" = " -fsanitize=thread"
sanitizerToFlags "" = ""
sanitizerToFlags s@_ = error $ "Unknown sanitizer: \"" ++ s ++ "\" expected one of [undefined, address, thread]"

buildTypeToFlags "debug" = "-g "
buildTypeToFlags "release" = "-g -O2 "
buildTypeToFlags "release-min" = "-O2 "
buildTypeToFlags s@_ = error $ "Unknown build type: \"" ++ s ++ "\" expected one of [debug, release, release-min]"

main = do
  argDict <- getArgDict
  let platform = T.pack $ getBuildPlatform argDict
  let buildType = getBuildType argDict
  let configuration = Configuration {
      platform = platform,
      buildType = T.pack buildType,
      exeExt = if platform == "mingw32" then ".exe" else "",
      soExt = if platform == "mingw32" then ".dll" else ".so",
      sanitizerFlags = sanitizerToFlags $ getSanitizer argDict,
      buildFlags = buildTypeToFlags buildType
    }
  dib $ targets configuration

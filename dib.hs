module Main where

import Dib
import Dib.Builders.C
import Dib.Target
import Data.Maybe
import Data.Monoid
import qualified Data.List as L
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
  cxxCompileFlags = "--std=c++17 -Wold-style-cast",
  linkFlags = "-shared -lpthread" <> sanitizerFlags config,
  outputLocation = ObjAndBinDirs ("obj/" <> platform config <> "-" <> buildType config) ("lib/" <> platform config <> "-" <> buildType config),
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
  cxxCompileFlags = "--std=c++17 -Wold-style-cast",
  linkFlags = "-L./lib/" <> platform config <> "-" <> buildType config <> " -llaminaFS -lstdc++ -lpthread" <> sanitizerFlags config,
  extraLinkDeps = ["lib/" <> platform config <> "-" <> buildType config <> "/liblaminaFS" <> soExt config],
  outputLocation = ObjAndBinDirs ("obj/" <> platform config <> "-" <> buildType config) ("bin/" <> platform config <> "-" <> buildType config),
  includeDirs = ["src", "tests"]
}

tests config = addDependency (makeCTarget $ testsInfo config) $ liblaminaFS config
cleanTests config = makeCleanTarget $ testsInfo config

-- Targets
allTarget config = makePhonyTarget "all" [liblaminaFS config, tests config]
targets config = [allTarget config,  liblaminaFS config, cleanLamina config, tests config, cleanTests config]

-- Configuration related functions
getBuildPlatform d = handleArgResult $ makeArgDictLookupFuncChecked "PLATFORM" "local" ["local", "mingw32"] d
getSanitizer d = handleArgResult $ makeArgDictLookupFuncChecked "SANITIZER" "" (map fst sanitizerMap) d
getBuildType d = handleArgResult $ makeArgDictLookupFuncChecked "BUILD" "release" (map fst buildTypeMap) d
getCompiler platform = if platform == "mingw32" then mingw32Config else defaultGXXConfig

findInMap m s = snd.fromJust $ L.find (\(k, v) -> k == s) m

sanitizerMap = [
  ("undefined", " -fsanitize=undefined"),
  ("address", " -fsanitize=address"),
  ("thread", " -fsanitize=thread"),
  ("", "")]
sanitizerToFlags = findInMap sanitizerMap

buildTypeMap = [
  ("debug", "-g "),
  ("release", "-g -O2 "),
  ("release-min", "-O2 ")]

buildTypeToFlags = findInMap buildTypeMap

handleArgResult (Left e) = error e
handleArgResult (Right v) = v

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

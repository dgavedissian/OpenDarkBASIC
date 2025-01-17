#pragma once

#include <filesystem>
#include <memory>
#include <ostream>

#include "odb-compiler/commands/CommandIndex.hpp"
#include "odb-compiler/commands/SDKType.hpp"
#include "odb-compiler/config.hpp"
#include "odb-compiler/ir/Node.hpp"

namespace odb::ir {
enum class OutputType
{
    LLVMIR,
    LLVMBitcode,
    ObjectFile
};

struct TargetTriple
{
    enum class Arch
    {
        i386,
        x86_64,
        AArch64,
    };

    enum class Platform
    {
        Windows,
        macOS,
        Linux,
    };

    Arch arch;
    Platform platform;

    std::string getLLVMTargetTriple() const
    {
        // Examples:
        //   i386-pc-windows-msvc
        //   x86_64-pc-linux-gnu
        std::string target_triple;
        switch (arch)
        {
        case TargetTriple::Arch::i386:
            target_triple += "i386";
            break;
        case TargetTriple::Arch::x86_64:
            target_triple += "x86_64";
            break;
        case TargetTriple::Arch::AArch64:
            target_triple += "aarch64";
            break;
        }
        target_triple += "-";
        switch (platform)
        {
        case TargetTriple::Platform::Windows:
            target_triple += "pc-windows-msvc";
            break;
        case TargetTriple::Platform::macOS:
            target_triple += "apple-darwin";
            break;
        case TargetTriple::Platform::Linux:
            target_triple += "pc-linux-gnu";
            break;
        }
        return target_triple;
    }
};

ODBCOMPILER_PUBLIC_API bool generateCode(SDKType sdkType, OutputType outputType, TargetTriple targetTriple,
                                         std::ostream& output, const std::string& moduleName, Program& program,
                                         const cmd::CommandIndex& cmdIndex);
ODBCOMPILER_PUBLIC_API bool linkExecutable(SDKType sdkType, const std::filesystem::path& sdkRootDir,
                                           const std::filesystem::path& linker, TargetTriple targetTriple,
                                           std::vector<std::string> inputFilenames, std::string& outputFilename);
} // namespace odb::ir
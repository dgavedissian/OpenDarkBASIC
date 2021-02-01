#include "odb-cli/Codegen.hpp"
#include "odb-cli/AST.hpp"
#include "odb-cli/Commands.hpp"
#include "odb-compiler/ir/Codegen.hpp"
#include "odb-compiler/ir/Node.hpp"
#include "odb-compiler/ir/SemanticChecker.hpp"

#include <fstream>

static odb::ir::OutputType outputType_ = odb::ir::OutputType::ObjectFile;

// ----------------------------------------------------------------------------
bool setOutputType(const std::vector<std::string>& args)
{
    if (args.empty())
    {
        return false;
    }

    if (args[0] == "llvm-ir")
    {
        outputType_ = odb::ir::OutputType::LLVMIR;
    }
    else if (args[0] == "llvm-bc")
    {
        outputType_ = odb::ir::OutputType::LLVMBitcode;
    }
    else if (args[0] == "obj")
    {
        outputType_ = odb::ir::OutputType::ObjectFile;
    }
    else if (args[0] == "exe")
    {
        outputType_ = odb::ir::OutputType::Executable;
    }

    return true;
}

// ----------------------------------------------------------------------------
bool output(const std::vector<std::string>& args)
{
    if (args.empty())
    {
        fprintf(stderr, "[codegen] An argument must be specified when generating an output file.\n");
        return false;
    }

    std::string outputName = args[0];
#ifdef _WIN32
    if (outputType_ == odb::ir::OutputType::Executable)
    {
        if (outputName.size() < 5 || outputName.substr(outputName.size() - 4, 4) != ".exe")
        {
            outputName += ".exe";
        }
    }
#endif

    std::ofstream outfile(outputName, std::ios::binary);
    if (!outfile.is_open())
    {
        fprintf(stderr, "[codegen] Error: Failed to open file `%s`\n", outputName.c_str());
        return false;
    }

    fprintf(stderr, "[codegen] Creating output file: `%s`\n", outputName.c_str());

    auto* cmdIndex = getCommandIndex();
    auto* ast = getAST();

    auto program = odb::ir::runSemanticChecks(ast, *cmdIndex);
    if (program)
    {
        odb::ir::generateCode(outputType_, outfile, "input.dba", *program, *cmdIndex);
        return true;
    }

    return false;
}
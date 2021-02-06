#pragma once

#include "odb-compiler/astpost/Process.hpp"

namespace odb::astpost {

class ODBCOMPILER_PUBLIC_API ResolveArrayFuncAmbiguity : public Process
{
public:
    bool execute(ast::Node* root) override final;
};

}

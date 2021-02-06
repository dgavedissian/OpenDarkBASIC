#include "ASTConverter.hpp"

#include "odb-compiler/ir/Node.hpp"

#include "odb-compiler/ast/ArrayDecl.hpp"
#include "odb-compiler/ast/ArrayRef.hpp"
#include "odb-compiler/ast/Assignment.hpp"
#include "odb-compiler/ast/BinaryOp.hpp"
#include "odb-compiler/ast/Block.hpp"
#include "odb-compiler/ast/Break.hpp"
#include "odb-compiler/ast/Command.hpp"
#include "odb-compiler/ast/Conditional.hpp"
#include "odb-compiler/ast/ConstDecl.hpp"
#include "odb-compiler/ast/Data.hpp"
#include "odb-compiler/ast/Datatypes.hpp"
#include "odb-compiler/ast/Decrement.hpp"
#include "odb-compiler/ast/Exporters.hpp"
#include "odb-compiler/ast/Expression.hpp"
#include "odb-compiler/ast/ExpressionList.hpp"
#include "odb-compiler/ast/FuncCall.hpp"
#include "odb-compiler/ast/FuncDecl.hpp"
#include "odb-compiler/ast/Goto.hpp"
#include "odb-compiler/ast/Increment.hpp"
#include "odb-compiler/ast/Label.hpp"
#include "odb-compiler/ast/Literal.hpp"
#include "odb-compiler/ast/Loop.hpp"
#include "odb-compiler/ast/Operators.hpp"
#include "odb-compiler/ast/SourceLocation.hpp"
#include "odb-compiler/ast/Statement.hpp"
#include "odb-compiler/ast/Subroutine.hpp"
#include "odb-compiler/ast/Symbol.hpp"
#include "odb-compiler/ast/UDTDecl.hpp"
#include "odb-compiler/ast/UDTRef.hpp"
#include "odb-compiler/ast/UnaryOp.hpp"
#include "odb-compiler/ast/VarDecl.hpp"
#include "odb-compiler/ast/VarRef.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>

namespace odb::ir {
namespace {
// TODO: Move this elsewhere.
template <typename... Args> [[noreturn]] void fatalError(const char* message, Args&&... args)
{
    fprintf(stderr, "FATAL ERROR:");
    fprintf(stderr, message, args...);
    std::terminate();
}
} // namespace

Type ASTConverter::getTypeFromAnnotation(Variable::Annotation annotation)
{
    switch (annotation)
    {
    case Variable::Annotation::None:
        return Type{BuiltinType::Integer};
    case Variable::Annotation::String:
        return Type{BuiltinType::String};
    case Variable::Annotation::Float:
        return Type{BuiltinType::Float};
    default:
        fatalError("getTypeFromAnnotation encountered unknown type.");
    }
}

Type ASTConverter::getTypeFromCommandType(cmd::Command::Type type)
{
    switch (type)
    {
    case cmd::Command::Type::Integer:
        return Type{BuiltinType::Integer};
    case cmd::Command::Type::Float:
        return Type{BuiltinType::Float};
    case cmd::Command::Type::String:
        return Type{BuiltinType::String};
    case cmd::Command::Type::Double:
        return Type{BuiltinType::DoubleFloat};
    case cmd::Command::Type::Long:
        return Type{BuiltinType::DoubleInteger};
    case cmd::Command::Type::Dword:
        return Type{BuiltinType::Dword};
    case cmd::Command::Type::Void:
        return Type{};
    default:
        fatalError("Unknown keyword type %c", (char)type);
    }
}

Variable::Annotation getAnnotation(ast::Symbol::Annotation astAnnotation)
{
    switch (astAnnotation)
    {
    case ast::Symbol::Annotation::NONE:
        return Variable::Annotation::None;
    case ast::Symbol::Annotation::STRING:
        return Variable::Annotation::String;
    case ast::Symbol::Annotation::FLOAT:
        return Variable::Annotation::Float;
    }

    assert(false);
}

Type ASTConverter::getBinaryOpCommonType(BinaryOp op, Expression* left, Expression* right)
{
    // TODO: Implement this properly.
    return left->getType();
}

Reference<Variable> ASTConverter::resolveVariableRef(const ast::VarRef* varRef)
{
    auto annotation = getAnnotation(varRef->symbol()->annotation());
    // TODO: This should take arguments into account as well.
    Reference<Variable> variable = currentFunction_->variables().lookup(varRef->symbol()->name(), annotation);
    if (!variable)
    {
        // If the variable doesn't exist, it gets implicitly declared with the annotation type.
        variable = new Variable(varRef->symbol()->location(), varRef->symbol()->name(), annotation,
                                getTypeFromAnnotation(annotation));
        currentFunction_->variables().add(variable);
    }
    return variable;
}

bool ASTConverter::isTypeConvertible(Type sourceType, Type targetType) const
{
    if (sourceType == targetType) {
        return true;
    }
    if (sourceType.isBuiltinType() && targetType.isBuiltinType())
    {
        // int -> int casts.
        if (isIntegralType(*sourceType.getBuiltinType()) && isIntegralType(*targetType.getBuiltinType()))
        {
            return true;
        }

        // fp -> fp casts.
        if (isFloatingPointType(*sourceType.getBuiltinType()) && isFloatingPointType(*targetType.getBuiltinType()))
        {
            return true;
        }

        // int -> fp casts.
        if (isIntegralType(*sourceType.getBuiltinType()) && isFloatingPointType(*targetType.getBuiltinType()))
        {
            return true;
        }

        // fp -> int casts.
        if (isFloatingPointType(*sourceType.getBuiltinType()) && isIntegralType(*targetType.getBuiltinType()))
        {
            return true;
        }
    }
    return false;
}

Ptr<Expression> ASTConverter::ensureType(Ptr<Expression> expression, Type targetType)
{
    Type expressionType = expression->getType();

    if (expressionType == targetType)
    {
        return expression;
    }

    // Handle builtin type conversions.
    if (isTypeConvertible(expressionType, targetType))
    {
        auto* location = expression->location();
        return std::make_unique<CastExpression>(location, std::move(expression), targetType);
    }

    // Unhandled cast. Runtime error.
    semanticError(expression->location(), "Failed to convert %s to %s.", expressionType.toString().c_str(),
                  targetType.toString().c_str());
    return expression;
}

FunctionCallExpression ASTConverter::convertCommandCallExpression(ast::SourceLocation* location,
                                                                  const std::string& commandName,
                                                                  const MaybeNull<ast::ExpressionList>& astArgs)
{
    // Extract arguments.
    PtrVector<Expression> args;
    if (astArgs.notNull())
    {
        for (ast::Expression* argExpr : astArgs->expressions())
        {
            args.emplace_back(convertExpression(argExpr));
        }
    }

    auto candidates = cmdIndex_.lookup(commandName);
    const cmd::Command* command = candidates.front();

    // If there are arguments, then we will need to perform overload resolution.
    if (!args.empty())
    {
        // Remove candidates which don't have the correct number of arguments.
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                        [&](const Reference<cmd::Command>& candidate)
                                        { return candidate->args().size() != args.size(); }),
                         candidates.end());

        // Remove candidates where arguments can not be converted.
        auto convertArgumentsNotPossiblePrecondition = [&](const Reference<cmd::Command>& candidate) -> bool
        {
            for (std::size_t i = 0; i < candidate->args().size(); ++i)
            {
                auto candidateArgCommandType = candidate->args()[i].type;
                if (candidateArgCommandType == cmd::Command::Type{'X'} ||
                    candidateArgCommandType == cmd::Command::Type{'A'})
                {
                    return true;
                }
                if (!isTypeConvertible(args[i]->getType(), getTypeFromCommandType(candidateArgCommandType)))
                {
                    return true;
                }
            }
            return false;
        };
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(), convertArgumentsNotPossiblePrecondition),
                         candidates.end());

        if (candidates.empty())
        {
            fatalError("Unable to find matching overload for keyword %s.", commandName.c_str());
        }

        // Sort candidates in ascending order by how suitable they are. The candidate at the end of the sorted list is
        // the best match.
        auto candidateRankFunction = [&](const Reference<cmd::Command>& candidateA,
                                         const Reference<cmd::Command>& candidateB) -> bool
        {
            // The candidate scoring function generates a score for a particular overload.
            // The goal should be that the best matching function should have the highest score.
            // The algorithm works as follows:
            // * Each argument contributes to the score. Various attributes add a certain score:
            //   * Exact match: +10
            //   * Same "archetype" (i.e. integers, floats): +1
            //
            // The result should be: We should prefer the overload with exactly the same argument. If it's not
            // exactly the same, then we should prefer the one with the same archetype. This means, if we have
            // a function "foo" with a int32 and double overload, and we call it with an int64 parameter,
            // the int32 overload should be preferred over the double. However, if we call it with a float
            // parameter, the double overload should be preferred.
            auto scoreCandidate = [&](const Reference<cmd::Command>& overload) -> int
            {
                int score = 0;
                for (std::size_t i = 0; i < overload->args().size(); ++i)
                {
                    auto overloadType = getTypeFromCommandType(overload->args()[i].type);
                    auto argType = args[i]->getType();
                    if (overloadType == argType)
                    {
                        score += 10;
                    }
                    else if (overloadType.isBuiltinType() && argType.isBuiltinType())
                    {
                        if (isIntegralType(*overloadType.getBuiltinType()) && isIntegralType(*argType.getBuiltinType()))
                        {
                            score += 1;
                        }
                        if (isFloatingPointType(*overloadType.getBuiltinType()) &&
                            isFloatingPointType(*argType.getBuiltinType()))
                        {
                            score += 1;
                        }
                    }
                }
                return score;
            };
            return scoreCandidate(candidateA) < scoreCandidate(candidateB);
        };
        std::sort(candidates.begin(), candidates.end(), candidateRankFunction);
        command = candidates.back();
    }

    // Now we have selected an overload, we may need to insert cast operations when passing arguments (in case the
    // overload is not perfect). Do that now.
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        args[i] = ensureType(std::move(args[i]), getTypeFromCommandType(command->args()[i].type));
    }

    return FunctionCallExpression{location, command, std::move(args), getTypeFromCommandType(command->returnType())};
}

FunctionCallExpression ASTConverter::convertFunctionCallExpression(ast::SourceLocation* location,
                                                                   ast::AnnotatedSymbol* symbol,
                                                                   const MaybeNull<ast::ExpressionList>& astArgs)
{
    // Lookup function.
    auto functionName = symbol->name();
    auto functionEntry = functionMap_.find(functionName);
    if (functionEntry == functionMap_.end())
    {
        semanticError(location, "Function %s is not defined.", functionName.c_str());
        std::terminate();
    }
    auto* functionDefinition = functionEntry->second.functionDefinition;

    // Verify argument list.
    if (!functionDefinition->arguments().empty() && astArgs.isNull())
    {
        semanticError(location, "Function '%s' requires %d arguments, but 0 were provided.", functionName.c_str(),
                      functionDefinition->arguments().size());
        std::terminate();
    }
    if (functionDefinition->arguments().size() != astArgs->expressions().size())
    {
        semanticError(location, "Function '%s' requires %d arguments, but 0 were provided.", functionName.c_str(),
                      functionDefinition->arguments().size(), astArgs->expressions().size());
        std::terminate();
    }

    PtrVector<Expression> args;
    if (astArgs.notNull())
    {
        const auto& functionDefArgs = functionDefinition->arguments();
        const auto& providedArgs = astArgs->expressions();
        for (int i = 0; i < providedArgs.size(); ++i)
        {
            args.emplace_back(ensureType(convertExpression(providedArgs[i]), functionDefArgs[i].type));
        }
    }

    Type returnType;
    if (functionDefinition->returnExpression())
    {
        returnType = functionDefinition->returnExpression()->getType();
    }

    return FunctionCallExpression{location, functionEntry->second.functionDefinition, std::move(args), returnType};
}

Ptr<Expression> ASTConverter::convertExpression(const ast::Expression* expression)
{
    auto* location = expression->location();
    if (auto* unaryOp = dynamic_cast<const ast::UnaryOp*>(expression))
    {
        UnaryOp unaryOpType = [](const ast::Expression* expression) -> UnaryOp
        {
#define X(op, tok)                                                                                                     \
    if (dynamic_cast<const ast::UnaryOp##op*>(expression))                                                             \
        return UnaryOp::op;
            ODB_UNARY_OP_LIST
#undef X
            fatalError("Unknown unary op.");
        }(expression);
        return std::make_unique<UnaryExpression>(location, unaryOpType, convertExpression(unaryOp->expr()));
    }
    else if (auto* binaryOp = dynamic_cast<const ast::BinaryOp*>(expression))
    {
        BinaryOp binaryOpType = [](const ast::Expression* expression) -> BinaryOp
        {
#define X(op, tok)                                                                                                     \
    if (dynamic_cast<const ast::BinaryOp##op*>(expression))                                                            \
        return BinaryOp::op;
            ODB_BINARY_OP_LIST
#undef X
            fatalError("Unknown binary op.");
        }(expression);
        auto lhs = convertExpression(binaryOp->lhs());
        auto rhs = convertExpression(binaryOp->rhs());
        auto commonType = getBinaryOpCommonType(binaryOpType, lhs.get(), rhs.get());
        return std::make_unique<BinaryExpression>(location, binaryOpType, ensureType(std::move(lhs), commonType),
                                                  ensureType(std::move(rhs), commonType));
    }
    else if (auto* varRef = dynamic_cast<const ast::VarRef*>(expression))
    {
        return std::make_unique<VarRefExpression>(location, resolveVariableRef(varRef));
    }
    else if (auto* literal = dynamic_cast<const ast::Literal*>(expression))
    {
#define X(dbname, cppname)                                                                                             \
    if (auto* ast##dbname##Literal = dynamic_cast<const ast::dbname##Literal*>(literal))                               \
        return std::make_unique<dbname##Literal>(literal->location(), ast##dbname##Literal->value());
        ODB_DATATYPE_LIST
#undef X
    }
    else if (auto* command = dynamic_cast<const ast::CommandExprSymbol*>(expression))
    {
        // TODO: Perform type checking of arguments.
        return std::make_unique<FunctionCallExpression>(
            convertCommandCallExpression(location, command->command(), command->args()));
    }
    else if (auto* funcCall = dynamic_cast<const ast::FuncCallExpr*>(expression))
    {
        return std::make_unique<FunctionCallExpression>(
            convertFunctionCallExpression(location, funcCall->symbol(), funcCall->args()));
    }
    fatalError("Unknown expression type");
}

Ptr<Statement> ASTConverter::convertStatement(ast::Statement* statement)
{
    auto* location = statement->location();
    if (auto* constDeclStatement = dynamic_cast<ast::ConstDecl*>(statement))
    {
        fatalError("Unimplemented ast::ConstDecl");
    }
    else if (auto* varDeclSt = dynamic_cast<ast::VarDecl*>(statement))
    {
        // Get var ref and type.
        Type varType;
        ast::Expression* initialValue = nullptr;
#define X(dbname, cppname)                                                                                             \
    if (auto* dbname##Ref = dynamic_cast<ast::dbname##VarDecl*>(varDeclSt))                                            \
    {                                                                                                                  \
        varType = Type{BuiltinType::dbname};                                                                           \
        initialValue = dbname##Ref->initialValue();                                                                    \
    }
        ODB_DATATYPE_LIST
#undef X
        // TODO: Implement UDTs.

        // If we're declaring a new variable, it must not exist already.
        auto annotation = getAnnotation(varDeclSt->symbol()->annotation());
        Reference<Variable> variable = currentFunction_->variables().lookup(varDeclSt->symbol()->name(), annotation);
        if (variable)
        {
            semanticError(varDeclSt->symbol()->location(), "Variable %s has already been declared as type %s.",
                          varDeclSt->symbol()->name().c_str(), variable->type().toString().c_str());
            semanticError(variable->location(), "See last declaration.");
            return nullptr;
        }

        // Declare new variable.
        variable = new Variable(varDeclSt->symbol()->location(), varDeclSt->symbol()->name(), annotation, varType);
        currentFunction_->variables().add(variable);

        return std::make_unique<VarAssignment>(location, currentFunction_, variable,
                                               ensureType(convertExpression(initialValue), varType));
    }
    else if (auto* assignmentSt = dynamic_cast<ast::VarAssignment*>(statement))
    {
        auto variable = resolveVariableRef(assignmentSt->variable());
        auto expression = ensureType(convertExpression(assignmentSt->expression()), variable->type());
        return std::make_unique<VarAssignment>(location, currentFunction_, std::move(variable), std::move(expression));
    }
    else if (auto* conditionalSt = dynamic_cast<ast::Conditional*>(statement))
    {
        return std::make_unique<Conditional>(
            location, currentFunction_,
            ensureType(convertExpression(conditionalSt->condition()), Type{BuiltinType::Boolean}),
            convertBlock(conditionalSt->trueBranch()), convertBlock(conditionalSt->falseBranch()));
    }
    else if (auto* subReturnSt = dynamic_cast<ast::SubReturn*>(statement))
    {
        // TODO: Find original label.
        return std::make_unique<SubReturn>(location, currentFunction_);
    }
    else if (auto* funcExitSt = dynamic_cast<ast::FuncExit*>(statement))
    {
        return std::make_unique<ExitFunction>(location, currentFunction_, convertExpression(funcExitSt->returnValue()));
    }
    else if (auto* whileLoopSt = dynamic_cast<ast::WhileLoop*>(statement))
    {
        return std::make_unique<WhileLoop>(location, currentFunction_,
                                           convertExpression(whileLoopSt->continueCondition()),
                                           convertBlock(whileLoopSt->body()));
    }
    else if (auto* untilLoopSt = dynamic_cast<ast::UntilLoop*>(statement))
    {
        return std::make_unique<UntilLoop>(location, currentFunction_, convertExpression(untilLoopSt->exitCondition()),
                                           convertBlock(untilLoopSt->body()));
    }
    else if (auto* infiniteLoopSt = dynamic_cast<ast::InfiniteLoop*>(statement))
    {
        return std::make_unique<InfiniteLoop>(location, currentFunction_, convertBlock(infiniteLoopSt->body()));
    }
    else if (auto* breakSt = dynamic_cast<ast::Break*>(statement))
    {
        return std::make_unique<Break>(location, currentFunction_);
    }
    else if (auto* labelSt = dynamic_cast<ast::Label*>(statement))
    {
        return std::make_unique<Label>(location, currentFunction_, labelSt->symbol()->name());
    }
    else if (auto* incVarSt = dynamic_cast<ast::IncrementVar*>(statement))
    {
        return std::make_unique<IncrementVar>(location, currentFunction_, resolveVariableRef(incVarSt->variable()),
                                              convertExpression(incVarSt->expression()));
    }
    else if (auto* decVarSt = dynamic_cast<ast::DecrementVar*>(statement))
    {
        return std::make_unique<DecrementVar>(location, currentFunction_, resolveVariableRef(decVarSt->variable()),
                                              convertExpression(decVarSt->expression()));
    }
    else if (auto* funcCallSt = dynamic_cast<ast::FuncCallStmnt*>(statement))
    {
        return std::make_unique<FunctionCall>(
            location, currentFunction_,
            convertFunctionCallExpression(funcCallSt->location(), funcCallSt->symbol(), funcCallSt->args()));
    }
    else if (auto* gotoSt = dynamic_cast<ast::GotoSymbol*>(statement))
    {
        return std::make_unique<Goto>(location, currentFunction_, gotoSt->labelSymbol()->name());
    }
    else if (auto* subCallSt = dynamic_cast<ast::SubCallSymbol*>(statement))
    {
        return std::make_unique<Gosub>(location, currentFunction_, subCallSt->labelSymbol()->name());
    }
    else if (auto* commandSt = dynamic_cast<ast::CommandStmntSymbol*>(statement))
    {
        return std::make_unique<FunctionCall>(
            location, currentFunction_,
            convertCommandCallExpression(commandSt->location(), commandSt->command(), commandSt->args()));
    }
    else
    {
        fatalError("Unknown statement type.");
    }
}

StatementBlock ASTConverter::convertBlock(const MaybeNull<ast::Block>& ast)
{
    StatementBlock block;
    if (ast.notNull())
    {
        for (ast::Statement* node : ast->statements())
        {
            block.emplace_back(convertStatement(node));
        }
    }
    return block;
}

StatementBlock ASTConverter::convertBlock(const std::vector<Reference<ast::Statement>>& ast)
{
    StatementBlock block;
    for (ast::Statement* node : ast)
    {
        block.emplace_back(convertStatement(node));
    }
    return block;
}

std::unique_ptr<FunctionDefinition> ASTConverter::convertFunctionWithoutBody(ast::FuncDecl* funcDecl)
{
    std::vector<FunctionDefinition::Argument> args;
    if (funcDecl->args().notNull())
    {
        for (ast::Expression* expression : funcDecl->args()->expressions())
        {
            // TODO: Should args()->expressions() be a list of VarRef's instead? Want to avoid the risk of nullptr here.
            auto* varRef = dynamic_cast<ast::VarRef*>(expression);
            assert(varRef);
            FunctionDefinition::Argument arg;
            arg.name = varRef->symbol()->name();
            arg.type = getTypeFromAnnotation(getAnnotation(varRef->symbol()->annotation()));
            args.emplace_back(arg);
        }
    }
    return std::make_unique<FunctionDefinition>(funcDecl->location(), funcDecl->symbol()->name(), std::move(args));
}

std::unique_ptr<Program> ASTConverter::generateProgram(const ast::Block* ast)
{
    bool reachedEndOfMain = false;

    PtrVector<FunctionDefinition> functionDefinitions;

    // Extract main function statements, and populate function table.
    std::vector<Reference<ast::Statement>> astMainStatements;
    for (const Reference<ast::Statement>& s : ast->statements())
    {
        auto* astFuncDecl = dynamic_cast<ast::FuncDecl*>(s.get());
        if (!astFuncDecl)
        {
            // If we've reached the end of the main function, we should only processing functions.
            if (reachedEndOfMain)
            {
                // TODO: Handle error properly.
                std::cerr << "We've reached the end of main, but encountered a node "
                             "that isn't a function.";
                std::terminate();
            }
            else
            {
                // Append to main block.
                astMainStatements.emplace_back(s);
            }
        }
        else
        {
            // We've reached the end of main once we hit a function declaration.
            reachedEndOfMain = true;

            // Generate function definition.
            functionDefinitions.emplace_back(convertFunctionWithoutBody(astFuncDecl));
            functionMap_.emplace(astFuncDecl->symbol()->name(),
                                 Function{astFuncDecl, functionDefinitions.back().get()});
        }
    }

    // Generate functions bodies.
    FunctionDefinition mainFunction(new ast::InlineSourceLocation("", "", 0, 0, 0, 0), "__DBMain");
    currentFunction_ = &mainFunction;
    mainFunction.appendStatements(convertBlock(astMainStatements));
    for (auto& [_, funcDetails] : functionMap_)
    {
        currentFunction_ = funcDetails.functionDefinition;
        funcDetails.functionDefinition->appendStatements(convertBlock(funcDetails.ast->body()->statements()));
        if (funcDetails.ast->returnValue().notNull())
        {
            funcDetails.functionDefinition->setReturnExpression(convertExpression(funcDetails.ast->returnValue()));
        }
    }

    if (errorOccurred_)
    {
        return nullptr;
    }
    return std::make_unique<Program>(std::move(mainFunction), std::move(functionDefinitions));
}
} // namespace odb::ir
#include "odb-compiler/ast/Assignment.hpp"
#include "odb-compiler/ast/Block.hpp"
#include "odb-compiler/ast/Expression.hpp"
#include "odb-compiler/ast/Loop.hpp"
#include "odb-compiler/ast/SourceLocation.hpp"
#include "odb-compiler/ast/Symbol.hpp"
#include "odb-compiler/ast/VarRef.hpp"
#include "odb-compiler/ast/Visitor.hpp"

namespace odb {
namespace ast {

// ----------------------------------------------------------------------------
Loop::Loop(SourceLocation* location) :
    Statement(location)
{
}

// ----------------------------------------------------------------------------
InfiniteLoop::InfiniteLoop(Block* body, SourceLocation* location) :
    Loop(location),
    body_(body)
{
    body->setParent(this);
}

// ----------------------------------------------------------------------------
InfiniteLoop::InfiniteLoop(SourceLocation* location) :
    Loop(location)
{
}

// ----------------------------------------------------------------------------
MaybeNull<Block> InfiniteLoop::body() const
{
    return body_.get();
}

// ----------------------------------------------------------------------------
void InfiniteLoop::accept(Visitor* visitor)
{
    visitor->visitInfiniteLoop(this);
    if (body_)
        body_->accept(visitor);
}
void InfiniteLoop::accept(ConstVisitor* visitor) const
{
    visitor->visitInfiniteLoop(this);
    if (body_)
        body_->accept(visitor);
}

// ----------------------------------------------------------------------------
void InfiniteLoop::swapChild(const Node* oldNode, Node* newNode)
{
    if (body_ == oldNode)
        body_ = dynamic_cast<Block*>(newNode);
    else
        assert(false);
}

// ----------------------------------------------------------------------------
WhileLoop::WhileLoop(Expression* continueCondition, Block* body, SourceLocation* location) :
    Loop(location),
    continueCondition_(continueCondition),
    body_(body)
{
    continueCondition->setParent(this);
    body->setParent(this);
}

// ----------------------------------------------------------------------------
WhileLoop::WhileLoop(Expression* continueCondition, SourceLocation* location) :
    Loop(location),
    continueCondition_(continueCondition)
{
    continueCondition->setParent(this);
}

// ----------------------------------------------------------------------------
Expression* WhileLoop::continueCondition() const
{
    return continueCondition_;
}

// ----------------------------------------------------------------------------
MaybeNull<Block> WhileLoop::body() const
{
    return body_.get();
}

// ----------------------------------------------------------------------------
void WhileLoop::accept(Visitor* visitor)
{
    visitor->visitWhileLoop(this);
    continueCondition_->accept(visitor);
    if (body_)
        body_->accept(visitor);
}
void WhileLoop::accept(ConstVisitor* visitor) const
{
    visitor->visitWhileLoop(this);
    continueCondition_->accept(visitor);
    if (body_)
        body_->accept(visitor);
}

// ----------------------------------------------------------------------------
void WhileLoop::swapChild(const Node* oldNode, Node* newNode)
{
    if (continueCondition_ == oldNode)
        continueCondition_ = dynamic_cast<Expression*>(newNode);
    else if (body_ == oldNode)
        body_ = dynamic_cast<Block*>(newNode);
    else
        assert(false);
}

// ----------------------------------------------------------------------------
UntilLoop::UntilLoop(Expression* exitCondition, Block* body, SourceLocation* location) :
    Loop(location),
    exitCondition_(exitCondition),
    body_(body)
{
    exitCondition->setParent(this);
    body->setParent(this);
}

// ----------------------------------------------------------------------------
UntilLoop::UntilLoop(Expression* exitCondition, SourceLocation* location) :
    Loop(location),
    exitCondition_(exitCondition)
{
    exitCondition->setParent(this);
}

// ----------------------------------------------------------------------------
Expression* UntilLoop::exitCondition() const
{
    return exitCondition_;
}

// ----------------------------------------------------------------------------
MaybeNull<Block> UntilLoop::body() const
{
    return body_.get();
}

// ----------------------------------------------------------------------------
void UntilLoop::accept(Visitor* visitor)
{
    visitor->visitUntilLoop(this);
    exitCondition_->accept(visitor);
    if (body_)
        body_->accept(visitor);
}
void UntilLoop::accept(ConstVisitor* visitor) const
{
    visitor->visitUntilLoop(this);
    exitCondition_->accept(visitor);
    if (body_)
        body_->accept(visitor);
}

// ----------------------------------------------------------------------------
void UntilLoop::swapChild(const Node* oldNode, Node* newNode)
{
    if (exitCondition_ == oldNode)
        exitCondition_ = dynamic_cast<Expression*>(newNode);
    else if (body_ == oldNode)
        body_ = dynamic_cast<Block*>(newNode);
    else
        assert(false);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, Expression* stepValue, AnnotatedSymbol* nextSymbol, Block* body, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue),
    stepValue_(stepValue),
    nextSymbol_(nextSymbol),
    body_(body)
{
    counter->setParent(this);
    endValue->setParent(this);
    stepValue->setParent(this);
    nextSymbol->setParent(this);
    body->setParent(this);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, Expression* stepValue, AnnotatedSymbol* nextSymbol, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue),
    stepValue_(stepValue),
    nextSymbol_(nextSymbol)
{
    counter->setParent(this);
    endValue->setParent(this);
    stepValue->setParent(this);
    nextSymbol->setParent(this);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, Expression* stepValue, Block* body, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue),
    stepValue_(stepValue),
    body_(body)
{
    counter->setParent(this);
    endValue->setParent(this);
    stepValue->setParent(this);
    body->setParent(this);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, Expression* stepValue, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue),
    stepValue_(stepValue)
{
    counter->setParent(this);
    endValue->setParent(this);
    stepValue->setParent(this);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, AnnotatedSymbol* nextSymbol, Block* body, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue),
    nextSymbol_(nextSymbol),
    body_(body)
{
    counter->setParent(this);
    endValue->setParent(this);
    nextSymbol->setParent(this);
    body->setParent(this);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, AnnotatedSymbol* nextSymbol, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue),
    nextSymbol_(nextSymbol)
{
    counter->setParent(this);
    endValue->setParent(this);
    nextSymbol->setParent(this);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, Block* body, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue),
    body_(body)
{
    counter->setParent(this);
    endValue->setParent(this);
    body->setParent(this);
}

// ----------------------------------------------------------------------------
ForLoop::ForLoop(VarAssignment* counter, Expression* endValue, SourceLocation* location) :
    Loop(location),
    counter_(counter),
    endValue_(endValue)
{
    counter->setParent(this);
    endValue->setParent(this);
}

// ----------------------------------------------------------------------------
VarAssignment* ForLoop::counter() const
{
    return counter_;
}

// ----------------------------------------------------------------------------
Expression* ForLoop::endValue() const
{
    return endValue_;
}

// ----------------------------------------------------------------------------
MaybeNull<Expression> ForLoop::stepValue() const
{
    return stepValue_.get();
}

// ----------------------------------------------------------------------------
MaybeNull<AnnotatedSymbol> ForLoop::nextSymbol() const
{
    return nextSymbol_.get();
}

// ----------------------------------------------------------------------------
MaybeNull<Block> ForLoop::body() const
{
    return body_.get();
}

// ----------------------------------------------------------------------------
void ForLoop::accept(Visitor* visitor)
{
    visitor->visitForLoop(this);

    counter_->accept(visitor);
    endValue_->accept(visitor);
    if (stepValue_)
        stepValue_->accept(visitor);
    if (nextSymbol_)
        nextSymbol_->accept(visitor);
    if (body_)
        body_->accept(visitor);
}
void ForLoop::accept(ConstVisitor* visitor) const
{
    visitor->visitForLoop(this);

    counter_->accept(visitor);
    endValue_->accept(visitor);
    if (stepValue_)
        stepValue_->accept(visitor);
    if (nextSymbol_)
        nextSymbol_->accept(visitor);
    if (body_)
        body_->accept(visitor);
}

// ----------------------------------------------------------------------------
void ForLoop::swapChild(const Node* oldNode, Node* newNode)
{
    if (counter_ == oldNode)
        counter_ = dynamic_cast<VarAssignment*>(newNode);
    else if (endValue_ == oldNode)
        endValue_ = dynamic_cast<Expression*>(newNode);
    else if (stepValue_ == oldNode)
        stepValue_ = dynamic_cast<Expression*>(newNode);
    else if (nextSymbol_ == oldNode)
        nextSymbol_ = dynamic_cast<AnnotatedSymbol*>(newNode);
    else if (body_ == oldNode)
        body_ = dynamic_cast<Block*>(newNode);
    else
        assert(false);
}

}
}
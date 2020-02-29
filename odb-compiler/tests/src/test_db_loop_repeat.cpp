#include <gmock/gmock.h>
#include "odbc/parsers/db/Driver.hpp"
#include "odbc/ast/Node.hpp"
#include "odbc/tests/ParserTestHarness.hpp"
#include <fstream>

#define NAME db_repeat

using namespace testing;

class NAME : public ParserTestHarness
{
public:
};

using namespace odbc;

TEST_F(NAME, infinite_loop)
{
    ASSERT_THAT(driver->parseString("repeat\nfoo()\nuntil cond\n"), IsTrue());
}

TEST_F(NAME, empty_loop)
{
    ASSERT_THAT(driver->parseString("repeat\nuntil cond\n"), IsTrue());
}

TEST_F(NAME, break_from_loop)
{
    ASSERT_THAT(driver->parseString("repeat\nbreak\nuntil cond\n"), IsTrue());
}

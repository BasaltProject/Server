#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/runtime/world/Dialogs.hpp>

#include <doctest.h>

namespace dialogs = basalt::server::runtime::world::dialogs;

TEST_CASE("a command template takes what the inputs hold, and nothing for a name it has no value for") {
    CHECK(dialogs::instantiate("give $(who) $(item) $(n)", {{"who", "Steve"}, {"item", "stone"}}) ==
          "give Steve stone ");
    CHECK(dialogs::escapeText("a\"b\n") == "a\\\"b\\n");
}

TEST_CASE("a slider with a step lands on a step counted from where it starts") {
    dialogs::Input input;
    input.kind = dialogs::InputKind::NumberRange;
    input.start = 0.0F;
    input.end = 10.0F;
    input.initialNumber = 2.0F;
    input.step = 3.0F;
    CHECK(dialogs::scaledNumber(input, 0.0F) == doctest::Approx(2.0F));
    CHECK(dialogs::scaledNumber(input, 1.0F) == doctest::Approx(8.0F));
    CHECK(dialogs::numberText(8.0F) == "8");
    CHECK(dialogs::numberText(8.5F) == "8.5");
}

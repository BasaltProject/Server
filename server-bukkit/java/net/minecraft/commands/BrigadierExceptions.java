// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import com.mojang.brigadier.exceptions.BuiltInExceptionProvider;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import net.minecraft.network.chat.Component;
public class BrigadierExceptions implements BuiltInExceptionProvider {
    private static Dynamic2CommandExceptionType range(String key) {
        return new Dynamic2CommandExceptionType((found, limit) -> Component.translatableEscape(key, limit, found));
    }
    private static DynamicCommandExceptionType one(String key) {
        return new DynamicCommandExceptionType(value -> Component.translatableEscape(key, value));
    }
    private static SimpleCommandExceptionType plain(String key) {
        return new SimpleCommandExceptionType(Component.translatable(key));
    }
    private static final Dynamic2CommandExceptionType kDoubleLow = range("argument.double.low");
    private static final Dynamic2CommandExceptionType kDoubleHigh = range("argument.double.big");
    private static final Dynamic2CommandExceptionType kFloatLow = range("argument.float.low");
    private static final Dynamic2CommandExceptionType kFloatHigh = range("argument.float.big");
    private static final Dynamic2CommandExceptionType kIntLow = range("argument.integer.low");
    private static final Dynamic2CommandExceptionType kIntHigh = range("argument.integer.big");
    private static final Dynamic2CommandExceptionType kLongLow = range("argument.long.low");
    private static final Dynamic2CommandExceptionType kLongHigh = range("argument.long.big");
    private static final DynamicCommandExceptionType kLiteral = one("argument.literal.incorrect");
    private static final SimpleCommandExceptionType kQuoteStart = plain("parsing.quote.expected.start");
    private static final SimpleCommandExceptionType kQuoteEnd = plain("parsing.quote.expected.end");
    private static final DynamicCommandExceptionType kEscape = one("parsing.quote.escape");
    private static final DynamicCommandExceptionType kBadBool = one("parsing.bool.invalid");
    private static final DynamicCommandExceptionType kBadInt = one("parsing.int.invalid");
    private static final SimpleCommandExceptionType kNoInt = plain("parsing.int.expected");
    private static final DynamicCommandExceptionType kBadLong = one("parsing.long.invalid");
    private static final SimpleCommandExceptionType kNoLong = plain("parsing.long.expected");
    private static final DynamicCommandExceptionType kBadDouble = one("parsing.double.invalid");
    private static final SimpleCommandExceptionType kNoDouble = plain("parsing.double.expected");
    private static final DynamicCommandExceptionType kBadFloat = one("parsing.float.invalid");
    private static final SimpleCommandExceptionType kNoFloat = plain("parsing.float.expected");
    private static final SimpleCommandExceptionType kNoBool = plain("parsing.bool.expected");
    private static final DynamicCommandExceptionType kSymbol = one("parsing.expected");
    private static final SimpleCommandExceptionType kUnknownCommand = plain("command.unknown.command");
    private static final SimpleCommandExceptionType kUnknownArgument = plain("command.unknown.argument");
    private static final SimpleCommandExceptionType kSeparator = plain("command.expected.separator");
    private static final DynamicCommandExceptionType kParse = one("command.exception");
    public Dynamic2CommandExceptionType doubleTooLow() { return kDoubleLow; }
    public Dynamic2CommandExceptionType doubleTooHigh() { return kDoubleHigh; }
    public Dynamic2CommandExceptionType floatTooLow() { return kFloatLow; }
    public Dynamic2CommandExceptionType floatTooHigh() { return kFloatHigh; }
    public Dynamic2CommandExceptionType integerTooLow() { return kIntLow; }
    public Dynamic2CommandExceptionType integerTooHigh() { return kIntHigh; }
    public Dynamic2CommandExceptionType longTooLow() { return kLongLow; }
    public Dynamic2CommandExceptionType longTooHigh() { return kLongHigh; }
    public DynamicCommandExceptionType literalIncorrect() { return kLiteral; }
    public SimpleCommandExceptionType readerExpectedStartOfQuote() { return kQuoteStart; }
    public SimpleCommandExceptionType readerExpectedEndOfQuote() { return kQuoteEnd; }
    public DynamicCommandExceptionType readerInvalidEscape() { return kEscape; }
    public DynamicCommandExceptionType readerInvalidBool() { return kBadBool; }
    public DynamicCommandExceptionType readerInvalidInt() { return kBadInt; }
    public SimpleCommandExceptionType readerExpectedInt() { return kNoInt; }
    public DynamicCommandExceptionType readerInvalidLong() { return kBadLong; }
    public SimpleCommandExceptionType readerExpectedLong() { return kNoLong; }
    public DynamicCommandExceptionType readerInvalidDouble() { return kBadDouble; }
    public SimpleCommandExceptionType readerExpectedDouble() { return kNoDouble; }
    public DynamicCommandExceptionType readerInvalidFloat() { return kBadFloat; }
    public SimpleCommandExceptionType readerExpectedFloat() { return kNoFloat; }
    public SimpleCommandExceptionType readerExpectedBool() { return kNoBool; }
    public DynamicCommandExceptionType readerExpectedSymbol() { return kSymbol; }
    public SimpleCommandExceptionType dispatcherUnknownCommand() { return kUnknownCommand; }
    public SimpleCommandExceptionType dispatcherUnknownArgument() { return kUnknownArgument; }
    public SimpleCommandExceptionType dispatcherExpectedArgumentSeparator() { return kSeparator; }
    public DynamicCommandExceptionType dispatcherParseException() { return kParse; }
}

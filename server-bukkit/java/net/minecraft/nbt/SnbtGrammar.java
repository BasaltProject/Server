// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.DynamicOps;
import java.nio.ByteBuffer;
import java.util.HexFormat;
import java.util.List;
import java.util.stream.IntStream;
import java.util.stream.LongStream;
import net.minecraft.util.parsing.packrat.Atom;
import net.minecraft.util.parsing.packrat.Dictionary;
import net.minecraft.util.parsing.packrat.NamedRule;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;
import net.minecraft.util.parsing.packrat.commands.Grammar;

/** The top rule hands the reader to the native-backed TagParser instead of expanding the grammar term by term. */
public class SnbtGrammar {
    private static final HexFormat kHexEscape = HexFormat.of().withUpperCase();

    public static String escapeControlCharacters(char c) {
        return switch (c) {
            case '\b' -> "b";
            case '\t' -> "t";
            case '\n' -> "n";
            case '\f' -> "f";
            case '\r' -> "r";
            default -> c < ' ' ? "x" + kHexEscape.toHexDigits((byte) c) : null;
        };
    }

    public static <T> Grammar<T> createParser(DynamicOps<T> ops) {
        TagParser<T> parser = TagParser.create(ops);
        Dictionary<StringReader> rules = new Dictionary<>();
        Atom<T> top = Atom.of("top");
        NamedRule<StringReader, T> named = rules.put(top, new Rule<StringReader, T>() {
            @Override
            public T parse(ParseState<StringReader> state) {
                StringReader reader = state.input();
                int start = reader.getCursor();
                try {
                    reader.skipWhitespace();
                    return parser.parseAsArgument(reader);
                } catch (CommandSyntaxException e) {
                    state.errorCollector().store(reader.getCursor(), e);
                    reader.setCursor(start);
                    return null;
                }
            }
        });
        return new Grammar<>(rules, named);
    }

    static enum Sign {
        PLUS, MINUS;

        public void append(StringBuilder output) {
            if (this == MINUS) {
                output.append('-');
            }
        }
    }

    static enum Base {
        BINARY, DECIMAL, HEX;
    }

    static enum TypeSuffix {
        FLOAT, DOUBLE, BYTE, SHORT, INT, LONG;
    }

    static enum SignedPrefix {
        SIGNED, UNSIGNED;
    }

    static record IntegerSuffix(SignedPrefix signed, TypeSuffix type) {
        public static final IntegerSuffix EMPTY = new IntegerSuffix(null, null);
    }

    static enum ArrayPrefix {
        BYTE(TypeSuffix.BYTE), INT(TypeSuffix.INT, TypeSuffix.BYTE, TypeSuffix.SHORT), LONG(TypeSuffix.LONG, TypeSuffix.BYTE, TypeSuffix.SHORT, TypeSuffix.INT);

        private final TypeSuffix defaultType;
        private final List<TypeSuffix> additional;

        ArrayPrefix(TypeSuffix defaultType, TypeSuffix... additional) {
            this.defaultType = defaultType;
            this.additional = List.of(additional);
        }

        public boolean isAllowed(TypeSuffix type) {
            return type == defaultType || additional.contains(type);
        }

        public <T> T create(DynamicOps<T> ops) {
            return switch (this) {
                case BYTE -> ops.createByteList(ByteBuffer.wrap(new byte[0]));
                case INT -> ops.createIntList(IntStream.empty());
                case LONG -> ops.createLongList(LongStream.empty());
            };
        }
    }

    static record Signed<T>(Sign sign, T value) {
    }
}

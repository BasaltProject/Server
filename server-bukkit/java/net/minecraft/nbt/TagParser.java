// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import java.util.ArrayList;
import java.util.List;

public class TagParser<T> {
    public static final SimpleCommandExceptionType ERROR_TRAILING_DATA =
        new SimpleCommandExceptionType(new LiteralMessage("Unexpected trailing data"));
    public static final SimpleCommandExceptionType ERROR_EXPECTED_COMPOUND =
        new SimpleCommandExceptionType(new LiteralMessage("Expected compound tag"));
    public static final char ELEMENT_SEPARATOR = ',';
    public static final char NAME_VALUE_SEPARATOR = ':';
    public static final Codec<CompoundTag> FLATTENED_CODEC = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(parseCompoundFully(text));
        } catch (CommandSyntaxException e) {
            return DataResult.error(e::getMessage);
        }
    }, CompoundTag::toString);
    public static final Codec<CompoundTag> LENIENT_CODEC = Codec.withAlternative(
        FLATTENED_CODEC, CompoundTag.CODEC);
    private final DynamicOps<T> ops;

    private TagParser(DynamicOps<T> ops) {
        this.ops = ops;
    }

    public DynamicOps<T> getOps() {
        return ops;
    }

    public static <T> TagParser<T> create(DynamicOps<T> ops) {
        return new TagParser<>(ops);
    }

    public static CompoundTag parseCompoundFully(String input) throws CommandSyntaxException {
        Tag tag = create(NbtOps.INSTANCE).parseFully(input);
        if (tag instanceof CompoundTag compound) {
            return compound;
        }
        throw ERROR_EXPECTED_COMPOUND.create();
    }

    public T parseFully(String input) throws CommandSyntaxException {
        return parseFully(new StringReader(input));
    }

    public T parseFully(StringReader reader) throws CommandSyntaxException {
        T value = convert(new Parser(reader).value());
        reader.skipWhitespace();
        if (reader.canRead()) {
            throw ERROR_TRAILING_DATA.createWithContext(reader);
        }
        return value;
    }

    public T parseAsArgument(StringReader reader) throws CommandSyntaxException {
        return convert(new Parser(reader).value());
    }

    public static CompoundTag parseCompoundAsArgument(StringReader reader) throws CommandSyntaxException {
        Tag tag = create(NbtOps.INSTANCE).parseAsArgument(reader);
        if (tag instanceof CompoundTag compound) {
            return compound;
        }
        throw ERROR_EXPECTED_COMPOUND.createWithContext(reader);
    }

    private T convert(Tag tag) {
        return ops == NbtOps.INSTANCE ? (T) tag : NbtOps.INSTANCE.convertTo(ops, tag);
    }

    private static final class Parser {
        private final StringReader reader;

        Parser(StringReader reader) {
            this.reader = reader;
        }

        private CommandSyntaxException error(String message) {
            return new SimpleCommandExceptionType(new LiteralMessage(message)).createWithContext(reader);
        }

        private void expect(char c) throws CommandSyntaxException {
            reader.skipWhitespace();
            if (!reader.canRead() || reader.peek() != c) {
                throw error("Expected '" + c + "'");
            }
            reader.skip();
        }

        Tag value() throws CommandSyntaxException {
            reader.skipWhitespace();
            if (!reader.canRead()) {
                throw error("Expected value");
            }
            char c = reader.peek();
            if (c == '{') {
                return compound();
            }
            if (c == '[') {
                return list();
            }
            if (c == '"' || c == '\'') {
                return StringTag.valueOf(quoted());
            }
            return scalar(unquoted());
        }

        private CompoundTag compound() throws CommandSyntaxException {
            expect('{');
            CompoundTag result = new CompoundTag();
            reader.skipWhitespace();
            if (reader.canRead() && reader.peek() == '}') {
                reader.skip();
                return result;
            }
            while (true) {
                reader.skipWhitespace();
                String key = reader.canRead() && (reader.peek() == '"' || reader.peek() == '\'') ? quoted() : unquoted();
                if (key.isEmpty()) {
                    throw error("Expected key");
                }
                expect(':');
                result.put(key, value());
                reader.skipWhitespace();
                if (reader.canRead() && reader.peek() == ',') {
                    reader.skip();
                    reader.skipWhitespace();
                    if (reader.canRead() && reader.peek() == '}') {
                        reader.skip();
                        return result;
                    }
                    continue;
                }
                expect('}');
                return result;
            }
        }

        private Tag list() throws CommandSyntaxException {
            expect('[');
            reader.skipWhitespace();
            if (reader.canRead(2) && reader.peek(1) == ';') {
                char kind = Character.toUpperCase(reader.peek());
                if (kind == 'B' || kind == 'I' || kind == 'L') {
                    reader.skip();
                    reader.skip();
                    return typedArray(kind);
                }
            }
            ListTag result = new ListTag();
            if (reader.canRead() && reader.peek() == ']') {
                reader.skip();
                return result;
            }
            while (true) {
                Tag element = value();
                if (!result.isEmpty() && result.get(0).getId() != element.getId()) {
                    throw error("Can't insert " + element + " into list of " + result.get(0));
                }
                result.addAndUnwrap(element);
                reader.skipWhitespace();
                if (reader.canRead() && reader.peek() == ',') {
                    reader.skip();
                    reader.skipWhitespace();
                    if (reader.canRead() && reader.peek() == ']') {
                        reader.skip();
                        return result;
                    }
                    continue;
                }
                expect(']');
                return result;
            }
        }

        private Tag typedArray(char kind) throws CommandSyntaxException {
            List<Number> numbers = new ArrayList<>();
            reader.skipWhitespace();
            if (!(reader.canRead() && reader.peek() == ']')) {
                while (true) {
                    Tag element = value();
                    if (!(element instanceof NumericTag number)) {
                        throw error("Expected a number in array");
                    }
                    numbers.add(number.box());
                    reader.skipWhitespace();
                    if (reader.canRead() && reader.peek() == ',') {
                        reader.skip();
                        continue;
                    }
                    break;
                }
            }
            expect(']');
            switch (kind) {
                case 'B': {
                    byte[] bytes = new byte[numbers.size()];
                    for (int i = 0; i < bytes.length; i++) {
                        bytes[i] = numbers.get(i).byteValue();
                    }
                    return new ByteArrayTag(bytes);
                }
                case 'I': {
                    int[] ints = new int[numbers.size()];
                    for (int i = 0; i < ints.length; i++) {
                        ints[i] = numbers.get(i).intValue();
                    }
                    return new IntArrayTag(ints);
                }
                default: {
                    long[] longs = new long[numbers.size()];
                    for (int i = 0; i < longs.length; i++) {
                        longs[i] = numbers.get(i).longValue();
                    }
                    return new LongArrayTag(longs);
                }
            }
        }

        private String quoted() throws CommandSyntaxException {
            char quote = reader.read();
            StringBuilder out = new StringBuilder();
            while (reader.canRead()) {
                char c = reader.read();
                if (c == quote) {
                    return out.toString();
                }
                if (c != '\\') {
                    out.append(c);
                    continue;
                }
                if (!reader.canRead()) {
                    break;
                }
                char escaped = reader.read();
                switch (escaped) {
                    case 'b' -> out.append('\b');
                    case 'f' -> out.append('\f');
                    case 'n' -> out.append('\n');
                    case 'r' -> out.append('\r');
                    case 's' -> out.append(' ');
                    case 't' -> out.append('\t');
                    case 'x' -> out.append((char) hex(2));
                    case 'u' -> out.append((char) hex(4));
                    case 'U' -> out.appendCodePoint(hex(8));
                    case '\\', '\'', '"' -> out.append(escaped);
                    default -> throw error("Invalid escape sequence '\\" + escaped + "'");
                }
            }
            throw error("Unclosed quoted string");
        }

        private int hex(int digits) throws CommandSyntaxException {
            int value = 0;
            for (int i = 0; i < digits; i++) {
                if (!reader.canRead() || Character.digit(reader.peek(), 16) < 0) {
                    throw error("Invalid hex escape");
                }
                value = value << 4 | Character.digit(reader.read(), 16);
            }
            return value;
        }

        private String unquoted() {
            int start = reader.getCursor();
            while (reader.canRead() && isUnquoted(reader.peek())) {
                reader.skip();
            }
            return reader.getString().substring(start, reader.getCursor());
        }

        private static boolean isUnquoted(char c) {
            return c >= '0' && c <= '9' || c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '_' || c == '-'
                || c == '.' || c == '+';
        }

        private Tag scalar(String text) throws CommandSyntaxException {
            if (text.isEmpty()) {
                throw error("Expected value");
            }
            if (text.equalsIgnoreCase("true")) {
                return ByteTag.ONE;
            }
            if (text.equalsIgnoreCase("false")) {
                return ByteTag.ZERO;
            }
            try {
                String digits = text.replace("_", "");
                char last = Character.toLowerCase(digits.charAt(digits.length() - 1));
                String body = digits.substring(0, digits.length() - 1);
                boolean hasPoint = digits.contains(".");
                boolean exponent = digits.matches("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[fFdD]?")
                    && (digits.contains("e") || digits.contains("E"));
                if (digits.matches("[-+]?[0-9]+")) {
                    long value = Long.parseLong(digits);
                    return value >= Integer.MIN_VALUE && value <= Integer.MAX_VALUE ? IntTag.valueOf((int) value)
                        : StringTag.valueOf(text);
                }
                if (digits.matches("[-+]?[0-9]+[bB]")) {
                    return ByteTag.valueOf((byte) Integer.parseInt(body));
                }
                if (digits.matches("[-+]?[0-9]+[sS]")) {
                    return ShortTag.valueOf((short) Integer.parseInt(body));
                }
                if (digits.matches("[-+]?[0-9]+[lL]")) {
                    return LongTag.valueOf(Long.parseLong(body));
                }
                if (digits.matches("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[fF]") && (hasPoint || exponent || last == 'f')) {
                    return FloatTag.valueOf(Float.parseFloat(body));
                }
                if (digits.matches("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[dD]")) {
                    return DoubleTag.valueOf(Double.parseDouble(body));
                }
                if (digits.matches("[-+]?[0-9]*\\.[0-9]+([eE][-+]?[0-9]+)?") || digits.matches("[-+]?[0-9]+\\.")
                    || digits.matches("[-+]?[0-9]+[eE][-+]?[0-9]+")) {
                    return DoubleTag.valueOf(Double.parseDouble(digits));
                }
            } catch (NumberFormatException e) {
                return StringTag.valueOf(text);
            }
            return StringTag.valueOf(text);
        }
    }
}

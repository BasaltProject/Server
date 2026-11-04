// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.io.DataInput;
import java.util.Optional;

public final class StringTag implements PrimitiveTag {
    private final String value;

    private StringTag(String value) {
        this.value = value;
    }

    public static StringTag valueOf(String value) {
        return new StringTag(value);
    }

    public String value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_STRING;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeUTF(value);
    }

    @Override
    public StringTag copy() {
        return this;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof StringTag tag && tag.value.equals(value);
    }

    @Override
    public int hashCode() {
        return value.hashCode();
    }

    @Override
    public String toString() {
        return new StringTagVisitor().visit(this);
    }
    public static final TagType<StringTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("STRING", "TAG_String",
        (in, acc) -> {
            acc.accountBytes(36);
            String text = in.readUTF();
            acc.accountBytes(2, text.length());
            return StringTag.valueOf(text);
        });
    public static void skipString(DataInput input) throws IOException {
        input.skipBytes(input.readUnsignedShort());
    }
    public int sizeInBytes() {
        return 36 + 2 * value.length();
    }
    public TagType<StringTag> getType() {
        return TYPE;
    }
    public Optional<String> asString() {
        return Optional.of(value);
    }
    public void accept(TagVisitor visitor) {
        visitor.visitString(this);
    }
    public static String quoteAndEscape(String input) {
        StringBuilder result = new StringBuilder();
        quoteAndEscape(input, result);
        return result.toString();
    }
    public static void quoteAndEscape(String input, StringBuilder result) {
        boolean hasDouble = input.indexOf('"') >= 0;
        boolean hasSingle = input.indexOf('\'') >= 0;
        char delimiter = hasDouble && !hasSingle ? '\'' : '"';
        result.append(delimiter);
        escapeWithoutQuotes(input, result, delimiter);
        result.append(delimiter);
    }
    public static String escapeWithoutQuotes(String input) {
        StringBuilder result = new StringBuilder();
        escapeWithoutQuotes(input, result);
        return result.toString();
    }
    public static void escapeWithoutQuotes(String input, StringBuilder result) {
        escapeWithoutQuotes(input, result, '\0');
    }

    private static void escapeWithoutQuotes(String input, StringBuilder result, char delimiter) {
        for (int i = 0; i < input.length(); i++) {
            char c = input.charAt(i);
            switch (c) {
                case '\\' -> result.append("\\\\");
                case '\b' -> result.append("\\b");
                case '\f' -> result.append("\\f");
                case '\n' -> result.append("\\n");
                case '\r' -> result.append("\\r");
                case '\t' -> result.append("\\t");
                default -> {
                    if (c == delimiter) {
                        result.append('\\');
                    }
                    result.append(c);
                }
            }
        }
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(value);
    }
}

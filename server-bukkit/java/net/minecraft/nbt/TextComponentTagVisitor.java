// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.Style;
public class TextComponentTagVisitor implements TagVisitor {
    private static final int kInlineListThreshold = 8;
    private static final int kMaxDepth = 64;
    private static final int kMaxLength = 128;
    private static final Pattern kSimpleValue = Pattern.compile("[A-Za-z0-9._+-]+");
    private static final Component kNewline = Component.literal("\n");
    private static final Component kSpacing = Component.literal(" ");
    private final String indentation;
    private final Styling styling;
    private final boolean sortKeys;
    private int indentDepth;
    private int depth;
    private final MutableComponent result = Component.empty();
    public TextComponentTagVisitor(String indentation) {
        this(indentation, RichStyling.INSTANCE);
    }
    public TextComponentTagVisitor(String indentation, Styling styling) {
        this(indentation, styling, false);
    }
    public TextComponentTagVisitor(String indentation, Styling styling, boolean sortKeys) {
        this.indentation = indentation;
        this.styling = styling;
        this.sortKeys = sortKeys;
    }
    public Component visit(Tag tag) {
        tag.accept(this);
        return result;
    }
    private TextComponentTagVisitor append(String text, Style style) {
        result.append((Component) Component.literal(text).withStyle(style));
        return this;
    }
    private TextComponentTagVisitor append(Component component) {
        result.append(component);
        return this;
    }
    private TextComponentTagVisitor append(Token token) {
        result.append(styling.token(token));
        return this;
    }
    private TextComponentTagVisitor number(String text, Token suffix) {
        append(text, styling.numberStyle());
        return suffix == null ? this : append(suffix);
    }
    public void visitString(StringTag tag) {
        String quoted = StringTag.quoteAndEscape(tag.value());
        Component quote = Component.literal(quoted.substring(0, 1));
        append(quote).append(quoted.substring(1, quoted.length() - 1), styling.stringStyle()).append(quote);
    }
    public void visitByte(ByteTag tag) {
        number(String.valueOf(tag.value()), Token.BYTE_SUFFIX);
    }
    public void visitShort(ShortTag tag) {
        number(String.valueOf(tag.value()), Token.SHORT_SUFFIX);
    }
    public void visitInt(IntTag tag) {
        number(String.valueOf(tag.value()), null);
    }
    public void visitLong(LongTag tag) {
        number(String.valueOf(tag.value()), Token.LONG_SUFFIX);
    }
    public void visitFloat(FloatTag tag) {
        number(String.valueOf(tag.value()), Token.FLOAT_SUFFIX);
    }
    public void visitDouble(DoubleTag tag) {
        number(String.valueOf(tag.value()), Token.DOUBLE_SUFFIX);
    }
    private void numberArray(Token prefix, int length, java.util.function.IntFunction<String> element, Token suffix) {
        append(Token.LIST_OPEN).append(prefix).append(Token.LIST_TYPE_SEPARATOR);
        for (int i = 0; i < length && i < kMaxLength; i++) {
            append(kSpacing).number(element.apply(i), suffix);
            if (i != length - 1) {
                append(Token.ELEMENT_SEPARATOR);
            }
        }
        if (length > kMaxLength) {
            append(Token.FOLDED);
        }
        append(Token.LIST_CLOSE);
    }
    public void visitByteArray(ByteArrayTag tag) {
        byte[] data = tag.getAsByteArray();
        numberArray(Token.BYTE_ARRAY_PREFIX, data.length, i -> String.valueOf(data[i]), Token.BYTE_SUFFIX);
    }
    public void visitIntArray(IntArrayTag tag) {
        int[] data = tag.getAsIntArray();
        numberArray(Token.INT_ARRAY_PREFIX, data.length, i -> String.valueOf(data[i]), null);
    }
    public void visitLongArray(LongArrayTag tag) {
        long[] data = tag.getAsLongArray();
        numberArray(Token.LONG_ARRAY_PREFIX, data.length, i -> String.valueOf(data[i]), Token.LONG_SUFFIX);
    }
    private static boolean shouldWrapListElements(ListTag list) {
        if (list.size() >= kInlineListThreshold) {
            return false;
        }
        for (Tag element : list) {
            if (!(element instanceof NumericTag)) {
                return true;
            }
        }
        return false;
    }
    public void visitList(ListTag tag) {
        if (tag.isEmpty()) {
            append(Token.LIST_OPEN).append(Token.LIST_CLOSE);
        } else if (depth >= kMaxDepth) {
            append(Token.LIST_OPEN).append(Token.FOLDED).append(Token.LIST_CLOSE);
        } else if (!shouldWrapListElements(tag)) {
            append(Token.LIST_OPEN);
            for (int i = 0; i < tag.size(); i++) {
                if (i != 0) {
                    append(Token.ELEMENT_SEPARATOR).append(kSpacing);
                }
                appendSubTag(tag.get(i), false);
            }
            append(Token.LIST_CLOSE);
        } else {
            append(Token.LIST_OPEN);
            if (!indentation.isEmpty()) {
                append(kNewline);
            }
            Component entryIndent = Component.literal(indentation.repeat(indentDepth + 1));
            Component spacing = indentation.isEmpty() ? kSpacing : kNewline;
            for (int i = 0; i < tag.size() && i < kMaxLength; i++) {
                append(entryIndent);
                appendSubTag(tag.get(i), true);
                if (i != tag.size() - 1) {
                    append(Token.ELEMENT_SEPARATOR).append(spacing);
                }
            }
            if (tag.size() > kMaxLength) {
                append(entryIndent).append(Token.FOLDED);
            }
            if (!indentation.isEmpty()) {
                append(kNewline).append((Component) Component.literal(indentation.repeat(indentDepth)));
            }
            append(Token.LIST_CLOSE);
        }
    }
    public void visitCompound(CompoundTag tag) {
        if (tag.isEmpty()) {
            append(Token.STRUCT_OPEN).append(Token.STRUCT_CLOSE);
        } else if (depth >= kMaxDepth) {
            append(Token.STRUCT_OPEN).append(Token.FOLDED).append(Token.STRUCT_CLOSE);
        } else {
            append(Token.STRUCT_OPEN);
            Collection<String> keys = tag.keySet();
            if (sortKeys) {
                List<String> sorted = new ArrayList<>(keys);
                Collections.sort(sorted);
                keys = sorted;
            }
            if (!indentation.isEmpty()) {
                append(kNewline);
            }
            Component entryIndent = Component.literal(indentation.repeat(indentDepth + 1));
            Component spacing = indentation.isEmpty() ? kSpacing : kNewline;
            var iterator = keys.iterator();
            while (iterator.hasNext()) {
                String key = iterator.next();
                append(entryIndent).append(escapedKey(key)).append(Token.NAME_VALUE_SEPARATOR).append(kSpacing);
                appendSubTag(tag.get(key), true);
                if (iterator.hasNext()) {
                    append(Token.ELEMENT_SEPARATOR).append(spacing);
                }
            }
            if (!indentation.isEmpty()) {
                append(kNewline).append((Component) Component.literal(indentation.repeat(indentDepth)));
            }
            append(Token.STRUCT_CLOSE);
        }
    }
    private void appendSubTag(Tag tag, boolean indent) {
        if (indent) {
            indentDepth++;
        }
        depth++;
        try {
            tag.accept(this);
        } finally {
            if (indent) {
                indentDepth--;
            }
            depth--;
        }
    }
    private Component escapedKey(String key) {
        if (kSimpleValue.matcher(key).matches()) {
            return Component.literal(key).withStyle(styling.keyStyle());
        }
        String quoted = StringTag.quoteAndEscape(key);
        String quote = quoted.substring(0, 1);
        Component inner = Component.literal(quoted.substring(1, quoted.length() - 1)).withStyle(styling.keyStyle());
        return Component.literal(quote).append(inner).append(quote);
    }
    public void visitEnd(EndTag tag) {
    }
    public static enum Token {
        FOLDED("<...>"), ELEMENT_SEPARATOR(","), LIST_CLOSE("]"), LIST_OPEN("["), LIST_TYPE_SEPARATOR(";"), STRUCT_CLOSE("}"),
        STRUCT_OPEN("{"), NAME_VALUE_SEPARATOR(":"), BYTE_SUFFIX("b"), BYTE_ARRAY_PREFIX("B"), SHORT_SUFFIX("s"),
        INT_ARRAY_PREFIX("I"), LONG_SUFFIX("L"), LONG_ARRAY_PREFIX("L"), FLOAT_SUFFIX("f"), DOUBLE_SUFFIX("d");
        public final String text;
        private Token(String text) {
            this.text = text;
        }
    }
    public static class RichStyling implements Styling {
        private static final Style kNumberType = Style.EMPTY.withColor(ChatFormatting.RED);
        private static final Style kKey = Style.EMPTY.withColor(ChatFormatting.AQUA);
        private static final Style kString = Style.EMPTY.withColor(ChatFormatting.GREEN);
        private static final Style kNumber = Style.EMPTY.withColor(ChatFormatting.GOLD);
        public static final Styling INSTANCE = new RichStyling();
        private final Map<Token, Component> tokens = new EnumMap<>(Token.class);
        private RichStyling() {
            for (Token token : Token.values()) {
                Style style = switch (token) {
                    case FOLDED -> Style.EMPTY.withColor(ChatFormatting.GRAY);
                    case BYTE_SUFFIX, BYTE_ARRAY_PREFIX, SHORT_SUFFIX, INT_ARRAY_PREFIX, LONG_SUFFIX, LONG_ARRAY_PREFIX, FLOAT_SUFFIX, DOUBLE_SUFFIX -> kNumberType;
                    default -> null;
                };
                tokens.put(token, style == null ? Component.literal(token.text) : Component.literal(token.text).withStyle(style));
            }
        }
        public Style keyStyle() {
            return kKey;
        }
        public Style stringStyle() {
            return kString;
        }
        public Style numberStyle() {
            return kNumber;
        }
        public Component token(Token token) {
            return tokens.get(token);
        }
    }
    public static class PlainStyling implements Styling {
        public static final Styling INSTANCE = new PlainStyling();
        private final Map<Token, Component> tokens = new EnumMap<>(Token.class);
        private PlainStyling() {
            for (Token token : Token.values()) {
                tokens.put(token, Component.literal(token.text));
            }
        }
        public Style keyStyle() {
            return Style.EMPTY;
        }
        public Style stringStyle() {
            return Style.EMPTY;
        }
        public Style numberStyle() {
            return Style.EMPTY;
        }
        public Component token(Token token) {
            return tokens.get(token);
        }
    }
    public interface Styling {
        Style keyStyle();
        Style stringStyle();
        Style numberStyle();
        Component token(Token token);
    }
}

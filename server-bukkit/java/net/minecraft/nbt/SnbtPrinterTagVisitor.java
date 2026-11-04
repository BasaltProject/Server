// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Pattern;

public class SnbtPrinterTagVisitor implements TagVisitor {
    private static final Pattern SIMPLE_KEY = Pattern.compile("[A-Za-z0-9._+-]+");
    private final String indentation;
    private final int depth;
    private final List<String> path;
    private String result = "";

    public SnbtPrinterTagVisitor() {
        this("    ", 0, List.of());
    }

    public SnbtPrinterTagVisitor(String indentation, int depth, List<String> path) {
        this.indentation = indentation;
        this.depth = depth;
        this.path = path;
    }

    public String visit(Tag tag) {
        tag.accept(this);
        return result;
    }

    @Override
    public void visitString(StringTag tag) {
        result = StringTag.quoteAndEscape(tag.value());
    }

    @Override
    public void visitByte(ByteTag tag) {
        result = tag.value() + "b";
    }

    @Override
    public void visitShort(ShortTag tag) {
        result = tag.value() + "s";
    }

    @Override
    public void visitInt(IntTag tag) {
        result = String.valueOf(tag.value());
    }

    @Override
    public void visitLong(LongTag tag) {
        result = tag.value() + "L";
    }

    @Override
    public void visitFloat(FloatTag tag) {
        result = tag.value() + "f";
    }

    @Override
    public void visitDouble(DoubleTag tag) {
        result = tag.value() + "d";
    }

    @Override
    public void visitByteArray(ByteArrayTag tag) {
        result = new StringTagVisitor().visit(tag);
    }

    @Override
    public void visitIntArray(IntArrayTag tag) {
        result = new StringTagVisitor().visit(tag);
    }

    @Override
    public void visitLongArray(LongArrayTag tag) {
        result = new StringTagVisitor().visit(tag);
    }

    @Override
    public void visitList(ListTag tag) {
        if (tag.isEmpty()) {
            result = "[]";
            return;
        }
        boolean nested = tag.get(0) instanceof CompoundTag || tag.get(0) instanceof ListTag;
        StringBuilder out = new StringBuilder("[");
        for (int i = 0; i < tag.size(); i++) {
            if (nested) {
                out.append('\n').append(indentation.repeat(depth + 1));
            } else if (i > 0) {
                out.append(", ");
            }
            List<String> childPath = new ArrayList<>(path);
            childPath.add("[" + i + "]");
            out.append(new SnbtPrinterTagVisitor(indentation, depth + 1, childPath).visit(tag.get(i)));
            if (nested && i < tag.size() - 1) {
                out.append(',');
            }
        }
        if (nested) {
            out.append('\n').append(indentation.repeat(depth));
        }
        result = out.append(']').toString();
    }

    @Override
    public void visitCompound(CompoundTag tag) {
        if (tag.isEmpty()) {
            result = "{}";
            return;
        }
        StringBuilder out = new StringBuilder("{");
        List<String> keys = getKeys(tag);
        for (int i = 0; i < keys.size(); i++) {
            String key = keys.get(i);
            List<String> childPath = new ArrayList<>(path);
            childPath.add(key);
            out.append('\n').append(indentation.repeat(depth + 1));
            out.append(SIMPLE_KEY.matcher(key).matches() ? key : StringTag.quoteAndEscape(key)).append(": ");
            out.append(new SnbtPrinterTagVisitor(indentation, depth + 1, childPath).visit(tag.get(key)));
            if (i < keys.size() - 1) {
                out.append(',');
            }
        }
        result = out.append('\n').append(indentation.repeat(depth)).append('}').toString();
    }

    protected List<String> getKeys(CompoundTag tag) {
        List<String> keys = new ArrayList<>(tag.keySet());
        Collections.sort(keys);
        return keys;
    }

    public String pathString() {
        return String.join(".", path);
    }

    protected static String handleEscapePretty(String input) {
        return StringTag.quoteAndEscape(input);
    }

    @Override
    public void visitEnd(EndTag tag) {
        result = "";
    }
}

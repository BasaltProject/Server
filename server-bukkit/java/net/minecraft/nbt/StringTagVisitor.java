// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.util.Map;
import java.util.regex.Pattern;

public class StringTagVisitor implements TagVisitor {
    private static final Pattern SIMPLE_KEY = Pattern.compile("[A-Za-z0-9._+-]+");
    private final StringBuilder builder = new StringBuilder();

    public String visit(Tag tag) {
        tag.accept(this);
        return build();
    }

    public String build() {
        return builder.toString();
    }

    public void visitString(StringTag tag) {
        StringTag.quoteAndEscape(tag.value(), builder);
    }

    public void visitByte(ByteTag tag) {
        builder.append(tag.value()).append('b');
    }

    public void visitShort(ShortTag tag) {
        builder.append(tag.value()).append('s');
    }

    public void visitInt(IntTag tag) {
        builder.append(tag.value());
    }

    public void visitLong(LongTag tag) {
        builder.append(tag.value()).append('L');
    }

    public void visitFloat(FloatTag tag) {
        builder.append(tag.value()).append('f');
    }

    public void visitDouble(DoubleTag tag) {
        builder.append(tag.value()).append('d');
    }

    public void visitByteArray(ByteArrayTag tag) {
        builder.append("[B;");
        byte[] data = tag.getAsByteArray();
        for (int i = 0; i < data.length; i++) {
            if (i != 0) {
                builder.append(',');
            }
            builder.append(data[i]).append('B');
        }
        builder.append(']');
    }

    public void visitIntArray(IntArrayTag tag) {
        builder.append("[I;");
        int[] data = tag.getAsIntArray();
        for (int i = 0; i < data.length; i++) {
            if (i != 0) {
                builder.append(',');
            }
            builder.append(data[i]);
        }
        builder.append(']');
    }

    public void visitLongArray(LongArrayTag tag) {
        builder.append("[L;");
        long[] data = tag.getAsLongArray();
        for (int i = 0; i < data.length; i++) {
            if (i != 0) {
                builder.append(',');
            }
            builder.append(data[i]).append('L');
        }
        builder.append(']');
    }

    public void visitList(ListTag tag) {
        builder.append('[');
        for (int i = 0; i < tag.size(); i++) {
            if (i != 0) {
                builder.append(',');
            }
            tag.get(i).accept(this);
        }
        builder.append(']');
    }

    public void visitCompound(CompoundTag tag) {
        builder.append('{');
        boolean first = true;
        for (Map.Entry<String, Tag> entry : tag.entrySet()) {
            if (!first) {
                builder.append(',');
            }
            first = false;
            String key = entry.getKey();
            if (SIMPLE_KEY.matcher(key).matches()) {
                builder.append(key);
            } else {
                StringTag.quoteAndEscape(key, builder);
            }
            builder.append(':');
            entry.getValue().accept(this);
        }
        builder.append('}');
    }

    public void visitEnd(EndTag tag) {
        builder.append("END");
    }
}

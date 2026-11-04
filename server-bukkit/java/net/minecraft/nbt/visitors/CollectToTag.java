// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt.visitors;
import java.util.ArrayDeque;
import java.util.Deque;
import net.minecraft.nbt.ByteArrayTag;
import net.minecraft.nbt.ByteTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.EndTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.IntArrayTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.LongArrayTag;
import net.minecraft.nbt.LongTag;
import net.minecraft.nbt.ShortTag;
import net.minecraft.nbt.StreamTagVisitor;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TagType;
public class CollectToTag implements StreamTagVisitor {
    private final Deque<ContainerBuilder> containerStack = new ArrayDeque<>();
    public CollectToTag() {
        containerStack.addLast(new RootBuilder());
    }
    public Tag getResult() {
        return containerStack.getFirst().build();
    }
    protected int depth() {
        return containerStack.size() - 1;
    }
    private StreamTagVisitor.ValueResult append(Tag tag) {
        containerStack.getLast().acceptValue(tag);
        return StreamTagVisitor.ValueResult.CONTINUE;
    }
    public StreamTagVisitor.ValueResult visitEnd() {
        return append(EndTag.INSTANCE);
    }
    public StreamTagVisitor.ValueResult visit(String value) {
        return append(StringTag.valueOf(value));
    }
    public StreamTagVisitor.ValueResult visit(byte value) {
        return append(ByteTag.valueOf(value));
    }
    public StreamTagVisitor.ValueResult visit(short value) {
        return append(ShortTag.valueOf(value));
    }
    public StreamTagVisitor.ValueResult visit(int value) {
        return append(IntTag.valueOf(value));
    }
    public StreamTagVisitor.ValueResult visit(long value) {
        return append(LongTag.valueOf(value));
    }
    public StreamTagVisitor.ValueResult visit(float value) {
        return append(FloatTag.valueOf(value));
    }
    public StreamTagVisitor.ValueResult visit(double value) {
        return append(DoubleTag.valueOf(value));
    }
    public StreamTagVisitor.ValueResult visit(byte[] value) {
        return append(new ByteArrayTag(value));
    }
    public StreamTagVisitor.ValueResult visit(int[] value) {
        return append(new IntArrayTag(value));
    }
    public StreamTagVisitor.ValueResult visit(long[] value) {
        return append(new LongArrayTag(value));
    }
    public StreamTagVisitor.ValueResult visitList(TagType<?> elementType, int size) {
        return StreamTagVisitor.ValueResult.CONTINUE;
    }
    public StreamTagVisitor.EntryResult visitElement(TagType<?> type, int index) {
        enterContainerIfNeeded(type);
        return StreamTagVisitor.EntryResult.ENTER;
    }
    public StreamTagVisitor.EntryResult visitEntry(TagType<?> type) {
        return StreamTagVisitor.EntryResult.ENTER;
    }
    public StreamTagVisitor.EntryResult visitEntry(TagType<?> type, String id) {
        containerStack.getLast().acceptKey(id);
        enterContainerIfNeeded(type);
        return StreamTagVisitor.EntryResult.ENTER;
    }
    private void enterContainerIfNeeded(TagType<?> type) {
        if (type == ListTag.TYPE) {
            containerStack.addLast(new ListBuilder());
        } else if (type == CompoundTag.TYPE) {
            containerStack.addLast(new CompoundBuilder());
        }
    }
    public StreamTagVisitor.ValueResult visitContainerEnd() {
        Tag built = containerStack.removeLast().build();
        if (built != null) {
            containerStack.getLast().acceptValue(built);
        }
        return StreamTagVisitor.ValueResult.CONTINUE;
    }
    public StreamTagVisitor.ValueResult visitRootEntry(TagType<?> type) {
        enterContainerIfNeeded(type);
        return StreamTagVisitor.ValueResult.CONTINUE;
    }
    private interface ContainerBuilder {
        default void acceptKey(String id) {
        }
        void acceptValue(Tag tag);
        Tag build();
    }
    private static class RootBuilder implements ContainerBuilder {
        private Tag result;
        public void acceptValue(Tag tag) {
            result = tag;
        }
        public Tag build() {
            return result;
        }
    }
    private static class CompoundBuilder implements ContainerBuilder {
        private final CompoundTag compound = new CompoundTag();
        private String lastId = "";
        public void acceptKey(String id) {
            lastId = id;
        }
        public void acceptValue(Tag tag) {
            compound.put(lastId, tag);
        }
        public Tag build() {
            return compound;
        }
    }
    private static class ListBuilder implements ContainerBuilder {
        private final ListTag list = new ListTag();
        public void acceptValue(Tag tag) {
            list.addAndUnwrap(tag);
        }
        public Tag build() {
            return list;
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt.visitors;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.HashSet;
import java.util.Set;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.StreamTagVisitor;
import net.minecraft.nbt.TagType;
public class CollectFields extends CollectToTag {
    private int fieldsToGetCount;
    private final Set<TagType<?>> wantedTypes = new HashSet<>();
    private final Deque<FieldTree> stack = new ArrayDeque<>();
    public CollectFields(FieldSelector... wantedFields) {
        this.fieldsToGetCount = wantedFields.length;
        FieldTree root = FieldTree.createRoot();
        for (FieldSelector wantedField : wantedFields) {
            root.addEntry(wantedField);
            wantedTypes.add(wantedField.type());
        }
        stack.push(root);
        wantedTypes.add(CompoundTag.TYPE);
    }
    public StreamTagVisitor.ValueResult visitRootEntry(TagType<?> type) {
        return type != CompoundTag.TYPE ? StreamTagVisitor.ValueResult.HALT : super.visitRootEntry(type);
    }
    public StreamTagVisitor.EntryResult visitEntry(TagType<?> type) {
        FieldTree current = stack.element();
        if (depth() > current.depth()) {
            return super.visitEntry(type);
        }
        if (fieldsToGetCount <= 0) {
            return StreamTagVisitor.EntryResult.BREAK;
        }
        return wantedTypes.contains(type) ? super.visitEntry(type) : StreamTagVisitor.EntryResult.SKIP;
    }
    public StreamTagVisitor.EntryResult visitEntry(TagType<?> type, String id) {
        FieldTree current = stack.element();
        if (depth() > current.depth()) {
            return super.visitEntry(type, id);
        }
        if (current.selectedFields().remove(id, type)) {
            fieldsToGetCount--;
            return super.visitEntry(type, id);
        }
        if (type == CompoundTag.TYPE) {
            FieldTree next = current.fieldsToRecurse().get(id);
            if (next != null) {
                stack.push(next);
                return super.visitEntry(type, id);
            }
        }
        return StreamTagVisitor.EntryResult.SKIP;
    }
    public StreamTagVisitor.ValueResult visitContainerEnd() {
        if (depth() == stack.element().depth()) {
            stack.pop();
        }
        return super.visitContainerEnd();
    }
    public int getMissingFieldCount() {
        return fieldsToGetCount;
    }
}

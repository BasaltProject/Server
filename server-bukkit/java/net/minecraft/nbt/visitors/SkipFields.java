// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt.visitors;
import java.util.ArrayDeque;
import java.util.Deque;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.StreamTagVisitor;
import net.minecraft.nbt.TagType;
public class SkipFields extends CollectToTag {
    private final Deque<FieldTree> stack = new ArrayDeque<>();
    public SkipFields(FieldSelector... wantedFields) {
        FieldTree root = FieldTree.createRoot();
        for (FieldSelector wantedField : wantedFields) {
            root.addEntry(wantedField);
        }
        stack.push(root);
    }
    public StreamTagVisitor.EntryResult visitEntry(TagType<?> type, String id) {
        FieldTree current = stack.element();
        if (current.isSelected(type, id)) {
            return StreamTagVisitor.EntryResult.SKIP;
        }
        if (type == CompoundTag.TYPE) {
            FieldTree next = current.fieldsToRecurse().get(id);
            if (next != null) {
                stack.push(next);
            }
        }
        return super.visitEntry(type, id);
    }
    public StreamTagVisitor.ValueResult visitContainerEnd() {
        if (depth() == stack.element().depth()) {
            stack.pop();
        }
        return super.visitContainerEnd();
    }
}

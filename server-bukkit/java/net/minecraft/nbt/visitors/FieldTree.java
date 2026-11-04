// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt.visitors;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.nbt.TagType;
public record FieldTree(int depth, Map<String, TagType<?>> selectedFields, Map<String, FieldTree> fieldsToRecurse) {
    private FieldTree(int depth) {
        this(depth, new HashMap<>(), new HashMap<>());
    }
    public static FieldTree createRoot() {
        return new FieldTree(1);
    }
    public void addEntry(FieldSelector field) {
        if (depth <= field.path().size()) {
            fieldsToRecurse.computeIfAbsent(field.path().get(depth - 1), name -> new FieldTree(depth + 1)).addEntry(field);
        } else {
            selectedFields.put(field.name(), field.type());
        }
    }
    public boolean isSelected(TagType<?> type, String id) {
        return type.equals(selectedFields().get(id));
    }
}

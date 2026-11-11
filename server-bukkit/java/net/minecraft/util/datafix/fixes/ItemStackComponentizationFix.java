// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.OptionalDynamic;
import java.util.Optional;
import java.util.Set;
import java.util.function.UnaryOperator;
public class ItemStackComponentizationFix extends DataFix {
    public ItemStackComponentizationFix(Schema outputSchema) {
        super(outputSchema, true);
    }
    public static Dynamic<?> fixProfile(Dynamic<?> dynamic) {
        Dynamic<?> stack = dynamic.emptyMap().set("id", dynamic.createString("minecraft:player_head")).set("Count", dynamic.createInt(1))
            .set("tag", dynamic.emptyMap().set("SkullOwner", dynamic));
        Dynamic<?> fixed = org.basaltmc.bukkit.facade.NativeStep.dynamic("item_stack", "ItemStackComponentizationFix", 38185, stack);
        return fixed.get("components").get("minecraft:profile").orElseEmptyMap();
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    static class ItemStackData {
        private final String item;
        private final int count;
        private Dynamic<?> components;
        private final Dynamic<?> remainder;
        private Dynamic<?> tag;
        private ItemStackData(String item, int count, Dynamic<?> remainder) {
            this.item = net.minecraft.util.datafix.schemas.NamespacedSchema.ensureNamespaced(item);
            this.count = count;
            this.components = remainder.emptyMap();
            this.tag = remainder.get("tag").orElseEmptyMap();
            this.remainder = remainder.remove("tag");
        }
        public static Optional<ItemStackData> read(Dynamic<?> dynamic) {
            Optional<String> id = dynamic.get("id").asString().result();
            Optional<Number> count = dynamic.get("Count").asNumber().result();
            if (id.isEmpty() || count.isEmpty()) {
                return Optional.empty();
            }
            return Optional.of(new ItemStackData(id.get(), count.get().intValue(), dynamic.remove("id").remove("Count")));
        }
        public OptionalDynamic<?> removeTag(String key) {
            OptionalDynamic<?> value = tag.get(key);
            tag = tag.remove(key);
            return value;
        }
        public void setComponent(String type, Dynamic<?> value) {
            components = components.set(type, value);
        }
        public void setComponent(String type, OptionalDynamic<?> optionalValue) {
            optionalValue.result().ifPresent(value -> setComponent(type, value));
        }
        public Dynamic<?> moveTagInto(String fromKey, Dynamic<?> target, String toKey) {
            Optional<? extends Dynamic<?>> value = removeTag(fromKey).result();
            return value.isPresent() ? target.set(toKey, value.get()) : target;
        }
        public void moveTagToComponent(String key, String type, Dynamic<?> defaultValue) {
            removeTag(key).result().filter(value -> !value.equals(defaultValue)).ifPresent(value -> setComponent(type, value));
        }
        public void moveTagToComponent(String key, String type) {
            removeTag(key).result().ifPresent(value -> setComponent(type, value));
        }
        public void fixSubTag(String key, boolean dontFixWhenFieldIsMissing, UnaryOperator<Dynamic<?>> function) {
            OptionalDynamic<?> value = tag.get(key);
            if (dontFixWhenFieldIsMissing && value.result().isEmpty()) {
                return;
            }
            Dynamic<?> fixed = function.apply(value.orElseEmptyMap());
            tag = fixed.equals(fixed.emptyMap()) ? tag.remove(key) : tag.set(key, fixed);
        }
        public Dynamic<?> write() {
            Dynamic<?> empty = tag.emptyMap();
            Dynamic<?> result = empty.set("id", tag.createString(item)).set("count", tag.createInt(count));
            if (!tag.equals(empty)) {
                components = components.set("minecraft:custom_data", tag);
            }
            if (!components.equals(empty)) {
                result = result.set("components", components);
            }
            return merged(result, remainder);
        }
        private static <T> Dynamic<T> merged(Dynamic<T> itemStack, Dynamic<?> remainder) {
            com.mojang.serialization.DynamicOps<T> ops = itemStack.getOps();
            return ops.getMap(itemStack.getValue()).flatMap(map -> ops.mergeToMap(remainder.convert(ops).getValue(), map))
                .map(value -> new Dynamic<>(ops, value)).result().orElse(itemStack);
        }
        public boolean is(String id) {
            return item.equals(id);
        }
        public boolean is(Set<String> ids) {
            return ids.contains(item);
        }
        public boolean hasComponent(String id) {
            return components.get(id).result().isPresent();
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.util.Util;
import net.minecraft.world.item.component.ItemAttributeModifiers;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextUser;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public class SetAttributesFunction extends LootItemConditionalFunction {
    private final List<Modifier> modifiers;
    private final boolean replace;
    public static final MapCodec<SetAttributesFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetAttributesFunction.class, "MAP_CODEC");
    private SetAttributesFunction(Optional<Holder<LootItemCondition>> condition, List<Modifier> modifiers, boolean replace) {
        super(condition);
        this.modifiers = List.copyOf(modifiers);
        this.replace = replace;
    }
    public MapCodec<SetAttributesFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        super.validate(context);
    }
    private ItemAttributeModifiers applyAll(LootContext context, ItemAttributeModifiers base) {
        ItemAttributeModifiers result = base;
        for (Modifier each : modifiers) {
            EquipmentSlotGroup slot = Util.getRandom(each.slots(), context.getRandom());
            result = result.withModifierAdded(each.attribute(), new AttributeModifier(each.id(), each.amount().value().getFloat(context), each.operation()), slot);
        }
        return result;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (replace) {
            itemStack.set(DataComponents.ATTRIBUTE_MODIFIERS, applyAll(context, ItemAttributeModifiers.EMPTY));
        } else {
            itemStack.update(DataComponents.ATTRIBUTE_MODIFIERS, ItemAttributeModifiers.EMPTY, existing -> applyAll(context, existing));
        }
        return itemStack;
    }
    public static ModifierBuilder modifier(Identifier id, Holder<Attribute> attribute, AttributeModifier.Operation operation, Holder<ContextFloatProvider> amount) {
        return new ModifierBuilder(id, attribute, operation, amount);
    }
    public static Builder setAttributes() {
        return new Builder();
    }
    public static class ModifierBuilder {
        private final Identifier key;
        private final Holder<Attribute> target;
        private final AttributeModifier.Operation op;
        private final Holder<ContextFloatProvider> value;
        private final java.util.Set<EquipmentSlotGroup> groups = java.util.EnumSet.noneOf(EquipmentSlotGroup.class);
        public ModifierBuilder(Identifier id, Holder<Attribute> attribute, AttributeModifier.Operation operation, Holder<ContextFloatProvider> amount) {
            this.key = id;
            this.target = attribute;
            this.op = operation;
            this.value = amount;
            
        }
        public ModifierBuilder forSlot(EquipmentSlotGroup slot) {
            groups.add(slot);
            return this;
        }
        public Modifier build() {
            return new Modifier(key, target, op, value, List.copyOf(groups));
        }
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private boolean swap;
        private final List<Modifier> collected = new java.util.ArrayList<>();
        public Builder(boolean replace) {
            this.swap = replace;
            
        }
        public Builder() {
            this(false);
            
        }
        protected Builder getThis() {
            return this;
        }
        public Builder withModifier(ModifierBuilder modifier) {
            collected.add(modifier.build());
            return this;
        }
        public LootItemFunction build() {
            return new SetAttributesFunction(getCondition(), collected, swap);
        }
    }
    static record Modifier(Identifier id, Holder<Attribute> attribute, AttributeModifier.Operation operation, Holder<ContextFloatProvider> amount, List<EquipmentSlotGroup> slots) implements LootContextUser {
        public static final Codec<Modifier> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.SetAttributesFunction.Modifier.class, "CODEC");
        public void validate(ValidationContext context) {
            LootContextUser.super.validate(context);
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.advancements.predicates.CollectionPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.item.component.ItemAttributeModifiers;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record AttributeModifiersPredicate(Optional<CollectionPredicate<ItemAttributeModifiers.Entry, EntryPredicate>> modifiers) implements SingleComponentItemPredicate<ItemAttributeModifiers> {
    public static final Codec<AttributeModifiersPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        CollectionPredicate.<ItemAttributeModifiers.Entry, EntryPredicate>codec(EntryPredicate.CODEC).optionalFieldOf("modifiers").forGetter(AttributeModifiersPredicate::modifiers)).apply(i, AttributeModifiersPredicate::new));
    @SuppressWarnings("unchecked")
    private static <T> Codec<T> codecOf(Class<T> type) {
        return (Codec<T>) ReflectiveCodecs.forType(type);
    }
    public DataComponentType<ItemAttributeModifiers> componentType() {
        return DataComponents.ATTRIBUTE_MODIFIERS;
    }
    public boolean matches(ItemAttributeModifiers value) {
        return modifiers.isEmpty() || modifiers.get().test(value.modifiers());
    }
    public static record EntryPredicate(Optional<HolderSet<Attribute>> attribute, Optional<Identifier> id, MinMaxBounds.Doubles amount, Optional<AttributeModifier.Operation> operation, Optional<EquipmentSlotGroup> slot) implements Predicate<ItemAttributeModifiers.Entry> {
        public static final Codec<EntryPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
            ReflectiveCodecs.holderSet(Attribute.class).optionalFieldOf("attribute").forGetter(EntryPredicate::attribute),
            Identifier.CODEC.optionalFieldOf("id").forGetter(EntryPredicate::id),
            MinMaxBounds.Doubles.CODEC.optionalFieldOf("amount", MinMaxBounds.Doubles.ANY).forGetter(EntryPredicate::amount),
            codecOf(AttributeModifier.Operation.class).optionalFieldOf("operation").forGetter(EntryPredicate::operation),
            codecOf(EquipmentSlotGroup.class).optionalFieldOf("slot").forGetter(EntryPredicate::slot)).apply(i, EntryPredicate::new));
        public boolean test(ItemAttributeModifiers.Entry value) {
            if (attribute.isPresent() && !HolderSets.contains(attribute.get(), value.attribute())) {
                return false;
            }
            if (id.isPresent() && !id.get().equals(value.modifier().id())) {
                return false;
            }
            if (!amount.matches(value.modifier().amount())) {
                return false;
            }
            if (operation.isPresent() && operation.get() != value.modifier().operation()) {
                return false;
            }
            return slot.isEmpty() || slot.get() == value.slot();
        }
    }
}

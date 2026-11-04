// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponents;

public record DamagePredicate(MinMaxBounds.Ints durability, MinMaxBounds.Ints damage) implements DataComponentPredicate {
    public static final Codec<DamagePredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        MinMaxBounds.Ints.CODEC.optionalFieldOf("durability", MinMaxBounds.Ints.ANY).forGetter(DamagePredicate::durability),
        MinMaxBounds.Ints.CODEC.optionalFieldOf("damage", MinMaxBounds.Ints.ANY).forGetter(DamagePredicate::damage)).apply(i, DamagePredicate::new));

    @Override
    public boolean matches(DataComponentGetter components) {
        Integer maxDamage = components instanceof net.minecraft.world.item.ItemStack stack && stack.isDamageableItem() ? Integer.valueOf(stack.getMaxDamage())
            : components.get(DataComponents.MAX_DAMAGE);
        if (maxDamage == null) {
            return false;
        }
        Integer taken = components.getOrDefault(DataComponents.DAMAGE, 0);
        return durability.matches(maxDamage - taken) && damage.matches(taken);
    }

    public static DamagePredicate durability(MinMaxBounds.Ints range) {
        return new DamagePredicate(range, MinMaxBounds.Ints.ANY);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.tags.TagKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.level.ItemLike;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;

public record ItemPredicate(Optional<HolderSet<Item>> items, MinMaxBounds.Ints count, DataComponentMatchers components) implements Predicate<ItemInstance> {
    public static final Codec<ItemPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        ReflectiveCodecs.holderSet(Item.class).optionalFieldOf("items").forGetter(ItemPredicate::items),
        MinMaxBounds.Ints.CODEC.optionalFieldOf("count", MinMaxBounds.Ints.ANY).forGetter(ItemPredicate::count),
        DataComponentMatchers.CODEC.forGetter(ItemPredicate::components)).apply(i, ItemPredicate::new));

    @Override
    public boolean test(ItemInstance itemStack) {
        if (items.isPresent() && !HolderSets.contains(items.get(), itemStack.typeHolder())) {
            return false;
        }
        return count.matches(itemStack.count()) && components.test(itemStack);
    }

    public static class Builder {
        private Optional<HolderSet<Item>> items = Optional.empty();
        private MinMaxBounds.Ints count = MinMaxBounds.Ints.ANY;
        private DataComponentMatchers components = DataComponentMatchers.ANY;

        public Builder() {}

        public static Builder item() {
            return new Builder();
        }

        public Builder of(HolderGetter<Item> lookup, ItemLike... items) {
            this.items = Optional.of(HolderSet.direct(item -> item.asItem().builtInRegistryHolder(), items));
            return this;
        }

        public Builder of(HolderGetter<Item> lookup, TagKey<Item> tag) {
            this.items = Optional.of(lookup.getOrThrow(tag));
            return this;
        }

        public Builder withCount(MinMaxBounds.Ints count) {
            this.count = count;
            return this;
        }

        public Builder withComponents(DataComponentMatchers components) {
            this.components = components;
            return this;
        }

        public ItemPredicate build() {
            return new ItemPredicate(items, count, components);
        }
    }
}

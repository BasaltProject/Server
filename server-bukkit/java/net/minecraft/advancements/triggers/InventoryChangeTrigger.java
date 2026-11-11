// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import java.util.stream.Stream;
import net.minecraft.core.HolderSet;
import net.minecraft.advancements.predicates.DataComponentMatchers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class InventoryChangeTrigger extends SimpleCriterionTrigger<InventoryChangeTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Inventory inventory, ItemStack changedItem) {
        int full = 0;
        int empty = 0;
        int occupied = 0;
        for (int slot = 0; slot < inventory.getContainerSize(); slot++) {
            ItemStack stack = inventory.getItem(slot);
            if (stack.isEmpty()) {
                empty++;
            } else {
                occupied++;
                full += stack.getCount() >= stack.getMaxStackSize() ? 1 : 0;
            }
        }
        int fullSlots = full;
        int emptySlots = empty;
        int occupiedSlots = occupied;
        trigger(player, instance -> instance.matches(inventory, changedItem, fullSlots, emptySlots, occupiedSlots));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Slots slots, List<ItemPredicate> items) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    Slots.CODEC.optionalFieldOf("slots", Slots.ANY).forGetter(TriggerInstance::slots),
    ItemPredicate.CODEC.listOf().optionalFieldOf("items", List.of()).forGetter(TriggerInstance::items)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> hasItems(ItemPredicate.Builder... items) {
        return hasItems(Stream.of(items).map(ItemPredicate.Builder::build).toArray(ItemPredicate[]::new));
        }
        public static Criterion<TriggerInstance> hasItems(ItemPredicate... items) {
        return CriteriaTriggers.INVENTORY_CHANGED.createCriterion(new TriggerInstance(TriggerKit.none(), Slots.ANY, List.of(items)));
        }
        public static Criterion<TriggerInstance> hasItems(ItemLike... items) {
        ItemPredicate[] predicates = new ItemPredicate[items.length];
        for (int n = 0; n < items.length; n++) {
            predicates[n] = new ItemPredicate(Optional.of(HolderSet.direct(items[n].asItem().builtInRegistryHolder())), MinMaxBounds.Ints.ANY, DataComponentMatchers.ANY);
        }
        return hasItems(predicates);
        }
        public boolean matches(Inventory inventory, ItemStack changedItem, int slotsFull, int slotsEmpty, int slotsOccupied) {
        if (!slots.matches(slotsFull, slotsEmpty, slotsOccupied)) {
            return false;
        }
        if (items.isEmpty()) {
            return true;
        }
        if (items.size() == 1) {
            return !changedItem.isEmpty() && items.get(0).test(changedItem);
        }
        List<ItemPredicate> remaining = new java.util.ArrayList<>(items);
        for (int slot = 0; slot < inventory.getContainerSize() && !remaining.isEmpty(); slot++) {
            ItemStack stack = inventory.getItem(slot);
            if (!stack.isEmpty()) {
                remaining.removeIf(predicate -> predicate.test(stack));
            }
        }
        return remaining.isEmpty();
        }
        public static record Slots(MinMaxBounds.Ints occupied, MinMaxBounds.Ints full, MinMaxBounds.Ints empty) {
            public static final Codec<Slots> CODEC = RecordCodecBuilder.create(i -> i.group(
    MinMaxBounds.Ints.CODEC.optionalFieldOf("occupied", MinMaxBounds.Ints.ANY).forGetter(Slots::occupied),
    MinMaxBounds.Ints.CODEC.optionalFieldOf("full", MinMaxBounds.Ints.ANY).forGetter(Slots::full),
    MinMaxBounds.Ints.CODEC.optionalFieldOf("empty", MinMaxBounds.Ints.ANY).forGetter(Slots::empty)).apply(i, Slots::new));
            public static final Slots ANY = new Slots(MinMaxBounds.Ints.ANY, MinMaxBounds.Ints.ANY, MinMaxBounds.Ints.ANY);
            public boolean matches(int slotsFull, int slotsEmpty, int slotsOccupied) {
        return full.matches(slotsFull) && empty.matches(slotsEmpty) && occupied.matches(slotsOccupied);

            }
        }
    }
}

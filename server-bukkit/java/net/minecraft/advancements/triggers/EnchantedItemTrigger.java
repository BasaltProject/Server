// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class EnchantedItemTrigger extends SimpleCriterionTrigger<EnchantedItemTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, ItemStack itemStack, int levels) {
        trigger(player, instance -> instance.matches(itemStack, levels));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<ItemPredicate> item, MinMaxBounds.Ints levels) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    ItemPredicate.CODEC.optionalFieldOf("item").forGetter(TriggerInstance::item),
    MinMaxBounds.Ints.CODEC.optionalFieldOf("levels", MinMaxBounds.Ints.ANY).forGetter(TriggerInstance::levels)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> enchantedItem() {
        return CriteriaTriggers.ENCHANTED_ITEM.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), MinMaxBounds.Ints.ANY));
        }
        public boolean matches(ItemStack itemStack, int levels) {
        return (item.isEmpty() || item.get().test(itemStack)) && this.levels.matches(levels);
        }
    }
}

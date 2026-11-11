// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class LootTableTrigger extends SimpleCriterionTrigger<LootTableTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, ResourceKey<LootTable> lootTable) {
        trigger(player, instance -> instance.matches(lootTable));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, HolderSet<LootTable> lootTable) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    ReflectiveCodecs.holderSet(net.minecraft.world.level.storage.loot.LootTable.class).fieldOf("loot_tables").forGetter(TriggerInstance::lootTable)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> lootTableUsed(Holder<LootTable> lootTable) {
        return lootTableUsed(HolderSet.direct(lootTable));
        }
        public static Criterion<TriggerInstance> lootTableUsed(HolderSet<LootTable> lootTable) {
        return CriteriaTriggers.GENERATE_LOOT.createCriterion(new TriggerInstance(TriggerKit.none(), lootTable));
        }
        public boolean matches(ResourceKey<LootTable> lootTable) {
        return this.lootTable.stream().anyMatch(table -> table.is(lootTable));
        }
    }
}

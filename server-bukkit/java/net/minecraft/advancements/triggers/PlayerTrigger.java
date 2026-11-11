// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import net.minecraft.advancements.predicates.BlockPredicate;
import net.minecraft.advancements.predicates.entity.EntityEquipmentPredicate;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.LocationPredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class PlayerTrigger extends SimpleCriterionTrigger<PlayerTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player) {
        trigger(player, instance -> true);
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> located(LocationPredicate.Builder location) {
        return located(EntityPredicate.Builder.entity().located(location));
        }
        public static Criterion<TriggerInstance> located(EntityPredicate.Builder player) {
        return CriteriaTriggers.LOCATION.createCriterion(new TriggerInstance(Optional.of(EntityPredicate.wrap(player))));
        }
        public static Criterion<TriggerInstance> located(Optional<EntityPredicate> player) {
        return CriteriaTriggers.LOCATION.createCriterion(new TriggerInstance(EntityPredicate.wrap(player)));
        }
        public static Criterion<TriggerInstance> sleptInBed() {
        return CriteriaTriggers.SLEPT_IN_BED.createCriterion(new TriggerInstance(TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> raidWon() {
        return CriteriaTriggers.RAID_WIN.createCriterion(new TriggerInstance(TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> avoidVibration() {
        return CriteriaTriggers.AVOID_VIBRATION.createCriterion(new TriggerInstance(TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> tick() {
        return CriteriaTriggers.TICK.createCriterion(new TriggerInstance(TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> walkOnBlockWithEquipment(HolderGetter<Block> blocks, HolderGetter<Item> items, Block stepOnBlock, Item requiredEquipment) {
        EntityPredicate.Builder wearing = EntityPredicate.Builder.entity()
            .equipment(EntityEquipmentPredicate.Builder.equipment().feet(ItemPredicate.Builder.item().of(items, requiredEquipment)))
            .steppingOn(LocationPredicate.Builder.location().setBlock(BlockPredicate.Builder.block().of(blocks, stepOnBlock)));
        return located(wearing);

        }
    }
}

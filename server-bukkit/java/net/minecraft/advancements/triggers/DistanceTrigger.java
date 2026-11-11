// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.DistancePredicate;
import net.minecraft.advancements.predicates.LocationPredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.phys.Vec3;
public class DistanceTrigger extends SimpleCriterionTrigger<DistanceTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Vec3 startPosition) {
        Vec3 here = player.position();
        trigger(player, instance -> instance.matches(player.level(), startPosition, here));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<LocationPredicate> startPosition, Optional<DistancePredicate> distance) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    LocationPredicate.CODEC.optionalFieldOf("start_position").forGetter(TriggerInstance::startPosition),
    DistancePredicate.CODEC.optionalFieldOf("distance").forGetter(TriggerInstance::distance)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> fallFromHeight(EntityPredicate.Builder player, DistancePredicate distance, LocationPredicate.Builder startPosition) {
        return CriteriaTriggers.FALL_FROM_HEIGHT.createCriterion(
            new TriggerInstance(Optional.of(EntityPredicate.wrap(player)), Optional.of(startPosition.build()), Optional.of(distance)));
        }
        public static Criterion<TriggerInstance> rideEntityInLava(EntityPredicate.Builder player, DistancePredicate distance) {
        return CriteriaTriggers.RIDE_ENTITY_IN_LAVA_TRIGGER.createCriterion(
            new TriggerInstance(Optional.of(EntityPredicate.wrap(player)), TriggerKit.none(), Optional.of(distance)));
        }
        public static Criterion<TriggerInstance> travelledThroughNether(DistancePredicate distance) {
        return CriteriaTriggers.NETHER_TRAVEL.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), Optional.of(distance)));
        }
        public boolean matches(ServerLevel level, Vec3 enteredPosition, Vec3 playerPosition) {
        if (startPosition.isPresent() && !startPosition.get().matches(level, enteredPosition.x, enteredPosition.y, enteredPosition.z)) {
            return false;
        }
        return distance.isEmpty() || distance.get().matches(enteredPosition.x, enteredPosition.y, enteredPosition.z, playerPosition.x, playerPosition.y, playerPosition.z);

        }
    }
}

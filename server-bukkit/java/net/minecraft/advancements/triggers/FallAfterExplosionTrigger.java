// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.DistancePredicate;
import net.minecraft.advancements.predicates.LocationPredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.phys.Vec3;
public class FallAfterExplosionTrigger extends SimpleCriterionTrigger<FallAfterExplosionTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Vec3 startPosition, Entity cause) {
        Vec3 here = player.position();
        LootContext causeContext = cause == null ? null : EntityPredicate.createContext(player, cause);
        trigger(player, instance -> instance.matches(player.level(), startPosition, here, causeContext));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<LocationPredicate> startPosition, Optional<DistancePredicate> distance, Optional<Holder<LootItemCondition>> cause) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    LocationPredicate.CODEC.optionalFieldOf("start_position").forGetter(TriggerInstance::startPosition),
    DistancePredicate.CODEC.optionalFieldOf("distance").forGetter(TriggerInstance::distance),
    TriggerKit.cond("cause").forGetter(TriggerInstance::cause)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> fallAfterExplosion(DistancePredicate distance, EntityPredicate.Builder cause) {
        return CriteriaTriggers.FALL_AFTER_EXPLOSION.createCriterion(
            new TriggerInstance(TriggerKit.none(), TriggerKit.none(), Optional.of(distance), Optional.of(EntityPredicate.wrap(cause))));
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "cause", cause);
        }
        public boolean matches(ServerLevel level, Vec3 enteredPosition, Vec3 playerPosition, LootContext cause) {
        if (startPosition.isPresent() && !startPosition.get().matches(level, enteredPosition.x, enteredPosition.y, enteredPosition.z)) {
            return false;
        }
        if (distance.isPresent() && !distance.get().matches(enteredPosition.x, enteredPosition.y, enteredPosition.z, playerPosition.x, playerPosition.y, playerPosition.z)) {
            return false;
        }
        return this.cause.isEmpty() || (cause != null && this.cause.get().value().test(cause));
        }
    }
}

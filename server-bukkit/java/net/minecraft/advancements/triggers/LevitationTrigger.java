// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.DistancePredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.phys.Vec3;
public class LevitationTrigger extends SimpleCriterionTrigger<LevitationTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Vec3 start, int duration) {
        trigger(player, instance -> instance.matches(player, start, duration));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<DistancePredicate> distance, MinMaxBounds.Ints duration) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    DistancePredicate.CODEC.optionalFieldOf("distance").forGetter(TriggerInstance::distance),
    MinMaxBounds.Ints.CODEC.optionalFieldOf("duration", MinMaxBounds.Ints.ANY).forGetter(TriggerInstance::duration)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> levitated(DistancePredicate distance) {
        return CriteriaTriggers.LEVITATION.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(distance), MinMaxBounds.Ints.ANY));
        }
        public boolean matches(ServerPlayer player, Vec3 start, int duration) {
        if (distance.isPresent() && !distance.get().matches(start.x, start.y, start.z, player.getX(), player.getY(), player.getZ())) {
            return false;
        }
        return this.duration.matches(duration);
        }
    }
}

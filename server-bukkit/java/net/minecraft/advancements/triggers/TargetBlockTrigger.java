// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.phys.Vec3;
public class TargetBlockTrigger extends SimpleCriterionTrigger<TargetBlockTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Entity projectile, Vec3 hitPosition, int signalStrength) {
        LootContext projectileContext = EntityPredicate.createContext(player, projectile);
        trigger(player, instance -> instance.matches(projectileContext, hitPosition, signalStrength));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, MinMaxBounds.Ints signalStrength, Optional<Holder<LootItemCondition>> projectile) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    MinMaxBounds.Ints.CODEC.optionalFieldOf("signal_strength", MinMaxBounds.Ints.ANY).forGetter(TriggerInstance::signalStrength),
    TriggerKit.cond("projectile").forGetter(TriggerInstance::projectile)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> targetHit(MinMaxBounds.Ints redstoneSignalStrength, Optional<Holder<LootItemCondition>> projectile) {
        return CriteriaTriggers.TARGET_BLOCK_HIT.createCriterion(new TriggerInstance(TriggerKit.none(), redstoneSignalStrength, projectile));
        }
        public boolean matches(LootContext projectile, Vec3 hitPosition, int signalStrength) {
        return this.signalStrength.matches(signalStrength) && TriggerKit.passes(this.projectile, projectile);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "projectile", projectile);
        }
    }
}

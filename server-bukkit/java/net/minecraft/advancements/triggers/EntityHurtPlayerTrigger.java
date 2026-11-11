// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
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
import net.minecraft.advancements.predicates.DamagePredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class EntityHurtPlayerTrigger extends SimpleCriterionTrigger<EntityHurtPlayerTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, DamageSource source, float originalDamage, float actualDamage, boolean blocked) {
        trigger(player, instance -> instance.matches(player, source, originalDamage, actualDamage, blocked));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<DamagePredicate> damage) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    DamagePredicate.CODEC.optionalFieldOf("damage").forGetter(TriggerInstance::damage)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> entityHurtPlayer() {
        return CriteriaTriggers.ENTITY_HURT_PLAYER.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> entityHurtPlayer(DamagePredicate damage) {
        return CriteriaTriggers.ENTITY_HURT_PLAYER.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(damage)));
        }
        public static Criterion<TriggerInstance> entityHurtPlayer(DamagePredicate.Builder damage) {
        return entityHurtPlayer(damage.build());
        }
        public boolean matches(ServerPlayer player, DamageSource source, float originalDamage, float actualDamage, boolean blocked) {
        return damage.isEmpty() || damage.get().matches(player, source, originalDamage, actualDamage, blocked);
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.DamagePredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class PlayerHurtEntityTrigger extends SimpleCriterionTrigger<PlayerHurtEntityTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Entity victim, DamageSource source, float originalDamage, float actualDamage, boolean blocked) {
        LootContext victimContext = EntityPredicate.createContext(player, victim);
        trigger(player, instance -> instance.matches(player, victimContext, source, originalDamage, actualDamage, blocked));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<DamagePredicate> damage, Optional<Holder<LootItemCondition>> entity) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    DamagePredicate.CODEC.optionalFieldOf("damage").forGetter(TriggerInstance::damage),
    TriggerKit.cond("entity").forGetter(TriggerInstance::entity)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> playerHurtEntity() {
        return CriteriaTriggers.PLAYER_HURT_ENTITY.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> playerHurtEntityWithDamage(Optional<DamagePredicate> damage) {
        return CriteriaTriggers.PLAYER_HURT_ENTITY.createCriterion(new TriggerInstance(TriggerKit.none(), damage, TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> playerHurtEntityWithDamage(DamagePredicate.Builder damage) {
        return playerHurtEntityWithDamage(Optional.of(damage.build()));
        }
        public static Criterion<TriggerInstance> playerHurtEntity(Optional<EntityPredicate> entity) {
        return CriteriaTriggers.PLAYER_HURT_ENTITY.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), EntityPredicate.wrap(entity)));
        }
        public static Criterion<TriggerInstance> playerHurtEntity(Optional<DamagePredicate> damage, Optional<EntityPredicate> entity) {
        return CriteriaTriggers.PLAYER_HURT_ENTITY.createCriterion(new TriggerInstance(TriggerKit.none(), damage, EntityPredicate.wrap(entity)));
        }
        public static Criterion<TriggerInstance> playerHurtEntity(DamagePredicate.Builder damage, Optional<EntityPredicate> entity) {
        return playerHurtEntity(Optional.of(damage.build()), entity);
        }
        public boolean matches(ServerPlayer player, LootContext victim, DamageSource source, float originalDamage, float actualDamage, boolean blocked) {
        if (damage.isPresent() && !damage.get().matches(player, source, originalDamage, actualDamage, blocked)) {
            return false;
        }
        return TriggerKit.passes(entity, victim);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "entity", entity);
        }
    }
}

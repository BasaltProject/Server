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
import net.minecraft.advancements.predicates.DamageSourcePredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class KilledTrigger extends SimpleCriterionTrigger<KilledTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Entity entity, DamageSource killingBlow) {
        LootContext victim = EntityPredicate.createContext(player, entity);
        trigger(player, instance -> instance.matches(player, victim, killingBlow));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<Holder<LootItemCondition>> entity, Optional<DamageSourcePredicate> killingBlow) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.cond("entity").forGetter(TriggerInstance::entity),
    DamageSourcePredicate.CODEC.optionalFieldOf("killing_blow").forGetter(TriggerInstance::killingBlow)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> playerKilledEntity(Optional<EntityPredicate> entity) {
        return playerKilledEntity(entity, TriggerKit.none());
        }
        public static Criterion<TriggerInstance> playerKilledEntity(EntityPredicate.Builder entity) {
        return playerKilledEntity(Optional.of(entity.build()), TriggerKit.none());
        }
        public static Criterion<TriggerInstance> playerKilledEntity() {
        return CriteriaTriggers.PLAYER_KILLED_ENTITY.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> playerKilledEntity(Optional<EntityPredicate> entity, Optional<DamageSourcePredicate> killingBlow) {
        return CriteriaTriggers.PLAYER_KILLED_ENTITY.createCriterion(new TriggerInstance(TriggerKit.none(), EntityPredicate.wrap(entity), killingBlow));
        }
        public static Criterion<TriggerInstance> playerKilledEntity(EntityPredicate.Builder entity, Optional<DamageSourcePredicate> killingBlow) {
        return playerKilledEntity(Optional.of(entity.build()), killingBlow);
        }
        public static Criterion<TriggerInstance> playerKilledEntity(Optional<EntityPredicate> entity, DamageSourcePredicate.Builder killingBlow) {
        return playerKilledEntity(entity, Optional.of(killingBlow.build()));
        }
        public static Criterion<TriggerInstance> playerKilledEntity(EntityPredicate.Builder entity, DamageSourcePredicate.Builder killingBlow) {
        return playerKilledEntity(Optional.of(entity.build()), Optional.of(killingBlow.build()));
        }
        public static Criterion<TriggerInstance> playerKilledEntityNearSculkCatalyst() {
        return CriteriaTriggers.KILL_MOB_NEAR_SCULK_CATALYST.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> entityKilledPlayer(Optional<EntityPredicate> entity) {
        return entityKilledPlayer(entity, TriggerKit.none());
        }
        public static Criterion<TriggerInstance> entityKilledPlayer(EntityPredicate.Builder entity) {
        return entityKilledPlayer(Optional.of(entity.build()), TriggerKit.none());
        }
        public static Criterion<TriggerInstance> entityKilledPlayer() {
        return CriteriaTriggers.ENTITY_KILLED_PLAYER.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> entityKilledPlayer(Optional<EntityPredicate> entity, Optional<DamageSourcePredicate> killingBlow) {
        return CriteriaTriggers.ENTITY_KILLED_PLAYER.createCriterion(new TriggerInstance(TriggerKit.none(), EntityPredicate.wrap(entity), killingBlow));
        }
        public static Criterion<TriggerInstance> entityKilledPlayer(EntityPredicate.Builder entity, Optional<DamageSourcePredicate> killingBlow) {
        return entityKilledPlayer(Optional.of(entity.build()), killingBlow);
        }
        public static Criterion<TriggerInstance> entityKilledPlayer(Optional<EntityPredicate> entity, DamageSourcePredicate.Builder killingBlow) {
        return entityKilledPlayer(entity, Optional.of(killingBlow.build()));
        }
        public static Criterion<TriggerInstance> entityKilledPlayer(EntityPredicate.Builder entity, DamageSourcePredicate.Builder killingBlow) {
        return entityKilledPlayer(Optional.of(entity.build()), Optional.of(killingBlow.build()));
        }
        public boolean matches(ServerPlayer player, LootContext entity, DamageSource killingBlow) {
        if (this.killingBlow.isPresent() && !this.killingBlow.get().matches(player, killingBlow)) {
            return false;
        }
        return TriggerKit.passes(this.entity, entity);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "entity", entity);
        }
    }
}

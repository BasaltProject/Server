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
import java.util.List;
import java.util.Optional;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class LightningStrikeTrigger extends SimpleCriterionTrigger<LightningStrikeTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, LightningBolt lightning, List<Entity> entitiesAround) {
        List<LootContext> around = entitiesAround.stream().map(entity -> EntityPredicate.createContext(player, entity)).toList();
        LootContext bolt = EntityPredicate.createContext(player, lightning);
        trigger(player, instance -> instance.matches(bolt, around));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<Holder<LootItemCondition>> lightning, Optional<Holder<LootItemCondition>> bystander) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.cond("lightning").forGetter(TriggerInstance::lightning),
    TriggerKit.cond("bystander").forGetter(TriggerInstance::bystander)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> lightningStrike(Optional<EntityPredicate> lightning, Optional<EntityPredicate> bystander) {
        return CriteriaTriggers.LIGHTNING_STRIKE.createCriterion(new TriggerInstance(TriggerKit.none(), EntityPredicate.wrap(lightning), EntityPredicate.wrap(bystander)));
        }
        public boolean matches(LootContext bolt, List<LootContext> entitiesAround) {
        if (!TriggerKit.passes(lightning, bolt)) {
            return false;
        }
        return bystander.isEmpty() || entitiesAround.stream().anyMatch(bystander.get().value());
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "lightning", lightning);
        TriggerKit.check(validator, "bystander", bystander);
        }
    }
}

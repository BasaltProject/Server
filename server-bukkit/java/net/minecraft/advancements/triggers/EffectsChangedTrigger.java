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
import net.minecraft.advancements.predicates.MobEffectsPredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class EffectsChangedTrigger extends SimpleCriterionTrigger<EffectsChangedTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Entity source) {
        LootContext sourceContext = source == null ? null : EntityPredicate.createContext(player, source);
        trigger(player, instance -> instance.matches(player, sourceContext));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<MobEffectsPredicate> effects, Optional<Holder<LootItemCondition>> source) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    MobEffectsPredicate.CODEC.optionalFieldOf("effects").forGetter(TriggerInstance::effects),
    TriggerKit.cond("source").forGetter(TriggerInstance::source)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> hasEffects(MobEffectsPredicate.Builder effects) {
        return CriteriaTriggers.EFFECTS_CHANGED.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(effects.build()), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> gotEffectsFrom(EntityPredicate.Builder source) {
        return CriteriaTriggers.EFFECTS_CHANGED.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), Optional.of(EntityPredicate.wrap(source))));
        }
        public boolean matches(ServerPlayer player, LootContext source) {
        if (effects.isPresent() && !effects.get().matches(player)) {
            return false;
        }
        return this.source.isEmpty() || (source != null && this.source.get().value().test(source));
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "source", source);
        }
    }
}

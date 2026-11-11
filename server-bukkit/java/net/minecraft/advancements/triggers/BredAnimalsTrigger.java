// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class BredAnimalsTrigger extends SimpleCriterionTrigger<BredAnimalsTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Animal parent, Animal partner, AgeableMob child) {
        LootContext first = EntityPredicate.createContext(player, parent);
        LootContext second = EntityPredicate.createContext(player, partner);
        LootContext offspring = child == null ? null : EntityPredicate.createContext(player, child);
        trigger(player, instance -> instance.matches(first, second, offspring));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<Holder<LootItemCondition>> parent, Optional<Holder<LootItemCondition>> partner, Optional<Holder<LootItemCondition>> child) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.cond("parent").forGetter(TriggerInstance::parent),
    TriggerKit.cond("partner").forGetter(TriggerInstance::partner),
    TriggerKit.cond("child").forGetter(TriggerInstance::child)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> bredAnimals() {
        return CriteriaTriggers.BRED_ANIMALS.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> bredAnimals(EntityPredicate.Builder child) {
        return CriteriaTriggers.BRED_ANIMALS.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none(), Optional.of(EntityPredicate.wrap(child))));
        }
        public static Criterion<TriggerInstance> bredAnimals(Optional<EntityPredicate> parent1, Optional<EntityPredicate> parent2, Optional<EntityPredicate> child) {
        return CriteriaTriggers.BRED_ANIMALS.createCriterion(
            new TriggerInstance(TriggerKit.none(), EntityPredicate.wrap(parent1), EntityPredicate.wrap(parent2), EntityPredicate.wrap(child)));
        }
        public boolean matches(LootContext parent, LootContext partner, LootContext child) {
        if (this.child.isPresent() && (child == null || !this.child.get().value().test(child))) {
            return false;
        }
        return (TriggerKit.passes(this.parent, parent) && TriggerKit.passes(this.partner, partner))
            || (TriggerKit.passes(this.parent, partner) && TriggerKit.passes(this.partner, parent));
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "parent", parent);
        TriggerKit.check(validator, "partner", partner);
        TriggerKit.check(validator, "child", child);
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
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
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class TameAnimalTrigger extends SimpleCriterionTrigger<TameAnimalTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Animal animal) {
        LootContext animalContext = EntityPredicate.createContext(player, animal);
        trigger(player, instance -> instance.matches(animalContext));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<Holder<LootItemCondition>> entity) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.cond("entity").forGetter(TriggerInstance::entity)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> tamedAnimal() {
        return CriteriaTriggers.TAME_ANIMAL.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> tamedAnimal(EntityPredicate.Builder entity) {
        return CriteriaTriggers.TAME_ANIMAL.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(EntityPredicate.wrap(entity))));
        }
        public boolean matches(LootContext animal) {
        return TriggerKit.passes(entity, animal);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "entity", entity);
        }
    }
}

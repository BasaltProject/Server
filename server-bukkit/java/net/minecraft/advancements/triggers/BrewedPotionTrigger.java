// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
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
import net.minecraft.core.Holder;
import net.minecraft.core.component.predicates.PotionsPredicate;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.alchemy.PotionContents;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class BrewedPotionTrigger extends SimpleCriterionTrigger<BrewedPotionTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, PotionContents potion) {
        trigger(player, instance -> instance.matches(potion));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<PotionsPredicate> potion) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    PotionsPredicate.CODEC.optionalFieldOf("potion").forGetter(TriggerInstance::potion)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> brewedPotion() {
        return CriteriaTriggers.BREWED_POTION.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none()));
        }
        public boolean matches(PotionContents potion) {
        return this.potion.isEmpty() || this.potion.get().matches(potion);
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ChanneledLightningTrigger extends SimpleCriterionTrigger<ChanneledLightningTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Collection<? extends Entity> victims) {
        List<LootContext> contexts = victims.stream().map(victim -> EntityPredicate.createContext(player, victim)).toList();
        trigger(player, instance -> instance.matches(contexts));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, List<Holder<LootItemCondition>> victims) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.conds("victims").forGetter(TriggerInstance::victims)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> channeledLightning(EntityPredicate.Builder... victims) {
        return CriteriaTriggers.CHANNELED_LIGHTNING.createCriterion(new TriggerInstance(TriggerKit.none(), EntityPredicate.wrap(victims)));
        }
        public boolean matches(Collection<? extends LootContext> victims) {
        return TriggerKit.passesAll(this.victims, victims);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.checkAll(validator, "victims", victims);
        }
    }
}

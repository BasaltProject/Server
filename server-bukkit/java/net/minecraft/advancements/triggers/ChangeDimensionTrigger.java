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
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ChangeDimensionTrigger extends SimpleCriterionTrigger<ChangeDimensionTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, ResourceKey<Level> from, ResourceKey<Level> to) {
        trigger(player, instance -> instance.matches(from, to));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<ResourceKey<Level>> from, Optional<ResourceKey<Level>> to) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    ResourceKey.codec(Registries.DIMENSION).optionalFieldOf("from").forGetter(TriggerInstance::from),
    ResourceKey.codec(Registries.DIMENSION).optionalFieldOf("to").forGetter(TriggerInstance::to)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> changedDimension() {
        return CriteriaTriggers.CHANGED_DIMENSION.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none()));
        }
        public static Criterion<TriggerInstance> changedDimension(ResourceKey<Level> from, ResourceKey<Level> to) {
        return CriteriaTriggers.CHANGED_DIMENSION.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(from), Optional.of(to)));
        }
        public static Criterion<TriggerInstance> changedDimensionTo(ResourceKey<Level> to) {
        return CriteriaTriggers.CHANGED_DIMENSION.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), Optional.of(to)));
        }
        public static Criterion<TriggerInstance> changedDimensionFrom(ResourceKey<Level> from) {
        return CriteriaTriggers.CHANGED_DIMENSION.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(from), TriggerKit.none()));
        }
        public boolean matches(ResourceKey<Level> from, ResourceKey<Level> to) {
        return (this.from.isEmpty() || this.from.get() == from) && (this.to.isEmpty() || this.to.get() == to);
        }
    }
}

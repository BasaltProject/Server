// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class FilledBucketTrigger extends SimpleCriterionTrigger<FilledBucketTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, ItemStack item) {
        TriggerKit.fireNative(this, player, null, item, -1.0);
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<ItemPredicate> item) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    ItemPredicate.CODEC.optionalFieldOf("item").forGetter(TriggerInstance::item)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> filledBucket(ItemPredicate.Builder item) {
        return CriteriaTriggers.FILLED_BUCKET.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(item.build())));
        }
        public boolean matches(ItemStack item) {
        return this.item.isEmpty() || this.item.get().test(item);
        }
    }
}

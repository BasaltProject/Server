// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.StatePropertiesPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class BeeNestDestroyedTrigger extends SimpleCriterionTrigger<BeeNestDestroyedTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, BlockState state, ItemStack itemStack, int numBeesInside) {
        trigger(player, instance -> instance.matches(state, itemStack, numBeesInside));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<HolderSet<Block>> block, Optional<StatePropertiesPredicate> state, Optional<ItemPredicate> item, MinMaxBounds.Ints beesInside) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    org.basaltmc.bukkit.facade.ReflectiveCodecs.holderSet(Block.class).optionalFieldOf("blocks").forGetter(TriggerInstance::block),
    StatePropertiesPredicate.CODEC.optionalFieldOf("state").forGetter(TriggerInstance::state),
    ItemPredicate.CODEC.optionalFieldOf("item").forGetter(TriggerInstance::item),
    MinMaxBounds.Ints.CODEC.optionalFieldOf("num_bees_inside", MinMaxBounds.Ints.ANY).forGetter(TriggerInstance::beesInside)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> destroyedBeeNest(HolderGetter<Block> blocks, Block block, ItemPredicate.Builder itemPredicate, MinMaxBounds.Ints numBeesInside) {
        return destroyedBeeNest(HolderSet.direct(block.builtInRegistryHolder()), itemPredicate, numBeesInside);
        }
        public static Criterion<TriggerInstance> destroyedBeeNest(HolderSet<Block> block, ItemPredicate.Builder itemPredicate, MinMaxBounds.Ints numBeesInside) {
        return CriteriaTriggers.BEE_NEST_DESTROYED.createCriterion(
            new TriggerInstance(TriggerKit.none(), Optional.of(block), TriggerKit.none(), Optional.of(itemPredicate.build()), numBeesInside));
        }
        public boolean matches(BlockState state, ItemStack itemStack, int numBeesInside) {
        boolean blockOk = block.isEmpty() || state.is(block.get());
        boolean stateOk = this.state.isEmpty() || this.state.get().matches(state);
        boolean itemOk = item.isEmpty() || item.get().test(itemStack);
        return blockOk && stateOk && itemOk && beesInside.matches(numBeesInside);
        }
    }
}

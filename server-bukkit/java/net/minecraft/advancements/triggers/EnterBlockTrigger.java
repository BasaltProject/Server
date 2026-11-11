// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
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
import net.minecraft.advancements.predicates.StatePropertiesPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class EnterBlockTrigger extends SimpleCriterionTrigger<EnterBlockTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, BlockState state) {
        trigger(player, instance -> instance.matches(state));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<HolderSet<Block>> block, Optional<StatePropertiesPredicate> state) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    org.basaltmc.bukkit.facade.ReflectiveCodecs.holderSet(Block.class).optionalFieldOf("blocks").forGetter(TriggerInstance::block),
    StatePropertiesPredicate.CODEC.optionalFieldOf("state").forGetter(TriggerInstance::state)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> entersBlock(HolderGetter<Block> blocks, Block block) {
        return entersBlock(HolderSet.direct(block.builtInRegistryHolder()));
        }
        public static Criterion<TriggerInstance> entersBlock(HolderSet<Block> block) {
        return CriteriaTriggers.ENTER_BLOCK.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(block), TriggerKit.none()));
        }
        public boolean matches(BlockState state) {
        return (block.isEmpty() || state.is(block.get())) && (this.state.isEmpty() || this.state.get().matches(state));
        }
    }
}

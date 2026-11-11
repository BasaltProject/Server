// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.advancements.predicates.BlockPredicate;
import net.minecraft.advancements.predicates.StatePropertiesPredicate;
import net.minecraft.core.HolderGetter;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.storage.loot.LootContext;
public record MatchBlock(BlockPredicate predicate) implements LootItemCondition {
    public static final MapCodec<MatchBlock> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.MatchBlock.class, "MAP_CODEC");
    public MapCodec<MatchBlock> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return predicate.willMatchBlockEntity() ? Set.of(LootContextParams.BLOCK_STATE, LootContextParams.BLOCK_ENTITY) : Set.of(LootContextParams.BLOCK_STATE);
    }
    public boolean test(LootContext context) {
        var state = context.getOptional(LootContextParams.BLOCK_STATE);
        var tile = context.getOptional(LootContextParams.BLOCK_ENTITY);
        return state != null && predicate.matchesState(state) && predicate.matchesBlockEntity(context.getLevel(), tile);
    }
    public static LootItemCondition.Builder blockMatches(BlockPredicate.Builder predicate) {
        return () -> new MatchBlock(predicate.build());
    }
    public static LootItemCondition.Builder blockMatches(HolderGetter<Block> lookup, Block block) {
        return blockMatches(BlockPredicate.Builder.block().of(lookup, block));
    }
    public static LootItemCondition.Builder blockMatches(HolderGetter<Block> lookup, Block block, StatePropertiesPredicate.Builder properties) {
        return blockMatches(BlockPredicate.Builder.block().of(lookup, block).setProperties(properties));
    }
}

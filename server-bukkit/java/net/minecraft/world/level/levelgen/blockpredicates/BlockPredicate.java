// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.BiPredicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Directional;
import net.minecraft.core.Vec3i;
import net.minecraft.core.HolderSet;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.Fluids;
public interface BlockPredicate extends BiPredicate<LevelAccessor, BlockPos> {
    Codec<BlockPredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate.class, "CODEC");
    BlockPredicate ONLY_IN_AIR_PREDICATE = matchesBlocks(Blocks.AIR);
    BlockPredicate ONLY_IN_AIR_OR_WATER_PREDICATE = matchesBlocks(Blocks.AIR, Blocks.WATER);
    BlockPredicateType<?> type();
    static BlockPredicate allOf(List<BlockPredicate> predicates) {
        return new AllOfPredicate(List.copyOf(predicates));
    }
    static BlockPredicate allOf(BlockPredicate... predicates) {
        return allOf(List.of(predicates));
    }
    static BlockPredicate allOf(BlockPredicate a, BlockPredicate b) {
        return allOf(List.of(a, b));
    }
    static BlockPredicate anyOf(List<BlockPredicate> predicates) {
        return new AnyOfPredicate(List.copyOf(predicates));
    }
    static BlockPredicate anyOf(BlockPredicate... predicates) {
        return anyOf(List.of(predicates));
    }
    static BlockPredicate anyOf(BlockPredicate a, BlockPredicate b) {
        return anyOf(List.of(a, b));
    }
    static BlockPredicate matchesBlocks(Vec3i offset, List<Block> blocks) {
        HolderSet<Block> set = HolderSet.direct(Block::builtInRegistryHolder, blocks);
        return new MatchingBlocksPredicate(offset, set);
    }
    static BlockPredicate matchesBlocks(Directional directional, Block... blocks) {
        Vec3i step = directional.getStep();
        return matchesBlocks(step, java.util.Arrays.asList(blocks));
    }
    static BlockPredicate matchesBlocks(Block... blocks) {
        return matchesBlocks(Vec3i.ZERO, java.util.Arrays.asList(blocks));
    }
    static BlockPredicate matchesTag(Vec3i offset, TagKey<Block> tag) {
        MatchingBlockTagPredicate built = new MatchingBlockTagPredicate(offset, tag);
        return built;
    }
    static BlockPredicate matchesTag(Directional directional, TagKey<Block> tag) {
        Vec3i step = directional.getStep();
        return matchesTag(step, tag);
    }
    static BlockPredicate matchesTag(TagKey<Block> tag) {
        return matchesTag(Vec3i.ZERO, tag);
    }
    static BlockPredicate matchesFluids(Directional directional, Fluid... fluids) {
        Vec3i step = directional.getStep();
        return matchesFluids(step, java.util.Arrays.asList(fluids));
    }
    static BlockPredicate matchesFluids(Vec3i offset, List<Fluid> fluids) {
        HolderSet<Fluid> set = HolderSet.direct(Fluid::builtInRegistryHolder, fluids);
        return new MatchingFluidsPredicate(offset, set);
    }
    static BlockPredicate matchesFluids(Fluid... fluids) {
        return matchesFluids(Vec3i.ZERO, java.util.Arrays.asList(fluids));
    }
    static BlockPredicate not(BlockPredicate predicate) {
        return new NotPredicate(predicate);
    }
    static BlockPredicate replaceable() {
        return replaceable(Vec3i.ZERO);
    }
    static BlockPredicate replaceable(Vec3i offset) {
        return new ReplaceablePredicate(offset);
    }
    static BlockPredicate wouldSurvive(Block block) {
        BlockState state = block.defaultBlockState();
        return new WouldSurvivePredicate(Vec3i.ZERO, state);
    }
    static BlockPredicate hasSturdyFace(Direction direction) {
        return hasSturdyFace(Vec3i.ZERO, direction);
    }
    static BlockPredicate hasSturdyFace(Directional offset, Direction direction) {
        return hasSturdyFace(offset.getStep(), direction);
    }
    static BlockPredicate hasSturdyFace(Vec3i offset, Direction direction) {
        return new HasSturdyFacePredicate(offset, direction);
    }
    static BlockPredicate solid(Directional directional) {
        return solid(directional.getStep());
    }
    static BlockPredicate solid(Vec3i offset) {
        return new SolidPredicate(offset);
    }
    static BlockPredicate solid() {
        return solid(Vec3i.ZERO);
    }
    static BlockPredicate noFluid() {
        return noFluid(Vec3i.ZERO);
    }
    static BlockPredicate noFluid(Directional offset) {
        return noFluid(offset.getStep());
    }
    static BlockPredicate noFluid(Vec3i offset) {
        return matchesFluids(offset, List.of(Fluids.EMPTY));
    }
    static BlockPredicate insideWorld(Vec3i offset) {
        InsideWorldBoundsPredicate built = new InsideWorldBoundsPredicate(offset);
        return built;
    }
    static BlockPredicate alwaysTrue() {
        return TrueBlockPredicate.INSTANCE;
    }
    static BlockPredicate unobstructed(Vec3i offset) {
        return new UnobstructedPredicate(offset);
    }
    static BlockPredicate unobstructed() {
        return unobstructed(Vec3i.ZERO);
    }
}

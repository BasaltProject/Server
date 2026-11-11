// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.world.level.block.Blocks;
import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
public abstract class TreeDecorator {
    private static final class Marker { }
    public static final Codec<TreeDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.treedecorators.TreeDecorator.class, "CODEC");
    protected abstract TreeDecoratorType<?> type();
    public abstract void place(Context context);
    public static final class Context {
        private final WorldGenLevel level;
        private final BiConsumer<BlockPos, BlockState> decorationSetter;
        private final RandomSource random;
        private final ObjectArrayList<BlockPos> logs;
        private final ObjectArrayList<BlockPos> leaves;
        private final ObjectArrayList<BlockPos> roots;
        public Context(WorldGenLevel level, BiConsumer<BlockPos, BlockState> decorationSetter, RandomSource random, Set<BlockPos> trunkSet, Set<BlockPos> foliageSet, Set<BlockPos> rootSet) {
            this.level = level;
            this.decorationSetter = decorationSetter;
            this.random = random;
            this.roots = new ObjectArrayList<>(rootSet);
            this.logs = new ObjectArrayList<>(trunkSet);
            this.leaves = new ObjectArrayList<>(foliageSet);
            java.util.Comparator<BlockPos> byHeight = java.util.Comparator.comparingInt(net.minecraft.core.Vec3i::getY);
            this.logs.sort(byHeight);
            this.leaves.sort(byHeight);
            this.roots.sort(byHeight);
        }
        public void placeVine(BlockPos pos, BooleanProperty direction) {
            setBlock(pos, Blocks.VINE.defaultBlockState().setValue(direction, true));
        }
        public void setBlock(BlockPos pos, BlockState state) {
            decorationSetter.accept(pos, state);
        }
        public boolean isAir(BlockPos pos) {
            return level.isStateAtPosition(pos, BlockState::isAir);
        }
        public boolean checkBlock(BlockPos pos, Predicate<BlockState> predicate) {
            return level.isStateAtPosition(pos, predicate);
        }
        public boolean isReplaceable(BlockPos pos) {
            return checkBlock(pos, BlockState::canBeReplaced);
        }
        public boolean isWaterOrWaterNearby(BlockPos pos) {
            Predicate<BlockState> water = state -> state.is(Blocks.WATER);
            return checkBlock(pos, water) || checkBlock(pos.east(), water) || checkBlock(pos.west(), water)
                || checkBlock(pos.north(), water) || checkBlock(pos.south(), water);
        }
        public WorldGenLevel level() {
            return level;
        }
        public RandomSource random() {
            return random;
        }
        public ObjectArrayList<BlockPos> logs() {
            return logs;
        }
        public ObjectArrayList<BlockPos> leaves() {
            return leaves;
        }
        public ObjectArrayList<BlockPos> roots() {
            return roots;
        }
    }
}

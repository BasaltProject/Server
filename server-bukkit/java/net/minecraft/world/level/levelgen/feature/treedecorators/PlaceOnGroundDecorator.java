// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import java.util.List;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public class PlaceOnGroundDecorator extends TreeDecorator {
    private final int tries;
    private final int radius;
    private final int height;
    private final Holder<BlockStateProvider> blockStateProvider;
    private void tryAbove(TreeDecorator.Context context, BlockPos pos) {
        BlockPos above = pos.above();
        boolean free = context.level().isStateAtPosition(above, state -> state.isAir() || state.is(Blocks.VINE));
        if (free && context.checkBlock(pos, BlockState::isSolidRender)
            && context.level().getHeightmapPos(Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, pos).getY() <= above.getY()) {
            context.setBlock(above, blockStateProvider.value().getState(context.level(), context.random(), above));
        }
    }
    public static final MapCodec<PlaceOnGroundDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.PlaceOnGroundDecorator.class, "CODEC");
    public PlaceOnGroundDecorator(int tries, int radius, int height, Holder<BlockStateProvider> blockStateProvider) {
        this.tries = tries;
        this.radius = radius;
        this.height = height;
        this.blockStateProvider = blockStateProvider;
    }
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.PLACE_ON_GROUND;
    }
    public void place(TreeDecorator.Context context) {
        List<BlockPos> bases = TreeFeature.getLowestTrunkOrRootOfTree(context);
        if (bases.isEmpty()) {
            return;
        }
        BlockPos first = bases.get(0);
        int floor = first.getY();
        int loX = first.getX();
        int hiX = first.getX();
        int loZ = first.getZ();
        int hiZ = first.getZ();
        for (BlockPos base : bases) {
            if (base.getY() == floor) {
                loX = Math.min(loX, base.getX());
                hiX = Math.max(hiX, base.getX());
                loZ = Math.min(loZ, base.getZ());
                hiZ = Math.max(hiZ, base.getZ());
            }
        }
        RandomSource random = context.random();
        BoundingBox area = new BoundingBox(loX, floor, loZ, hiX, floor, hiZ).inflatedBy(radius, height, radius);
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int attempt = 0; attempt < tries; attempt++) {
            cursor.set(random.nextIntBetweenInclusive(area.minX(), area.maxX()), random.nextIntBetweenInclusive(area.minY(), area.maxY()),
                random.nextIntBetweenInclusive(area.minZ(), area.maxZ()));
            tryAbove(context, cursor);
        }
    }
}

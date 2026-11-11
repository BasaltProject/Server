// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import java.util.List;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public class AlterGroundDecorator extends TreeDecorator {
    private final Holder<BlockStateProvider> provider;
    private void disc(TreeDecorator.Context context, BlockPos center) {
        for (int dx = -2; dx <= 2; dx++) {
            for (int dz = -2; dz <= 2; dz++) {
                if (Math.abs(dx) != 2 || Math.abs(dz) != 2) {
                    setGround(context, center.offset(dx, 0, dz));
                }
            }
        }
    }
    private void setGround(TreeDecorator.Context context, BlockPos column) {
        for (int dy = 2; dy >= -3; dy--) {
            BlockPos at = column.above(dy);
            BlockState made = provider.value().getOptionalState(context.level(), context.random(), at);
            if (made != null) {
                context.setBlock(at, made);
                return;
            }
            if (!context.isAir(at) && dy < 0) {
                return;
            }
        }
    }
    public static final MapCodec<AlterGroundDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.AlterGroundDecorator.class, "CODEC");
    public AlterGroundDecorator(Holder<BlockStateProvider> provider) {
        this.provider = provider;
    }
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.ALTER_GROUND;
    }
    public void place(TreeDecorator.Context context) {
        List<BlockPos> bases = TreeFeature.getLowestTrunkOrRootOfTree(context);
        if (bases.isEmpty()) {
            return;
        }
        int lowest = bases.get(0).getY();
        for (BlockPos base : bases) {
            if (base.getY() != lowest) {
                continue;
            }
            disc(context, base.west().north());
            disc(context, base.east(2).north());
            disc(context, base.west().south(2));
            disc(context, base.east(2).south(2));
            for (int attempt = 0; attempt < 5; attempt++) {
                int slot = context.random().nextInt(64);
                int col = slot % 8;
                int row = slot / 8;
                if (col == 0 || col == 7 || row == 0 || row == 7) {
                    disc(context, base.offset(-3 + col, 0, -3 + row));
                }
            }
        }
    }
}

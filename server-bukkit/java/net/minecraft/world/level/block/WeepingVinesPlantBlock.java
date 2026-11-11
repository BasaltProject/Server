// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.BlockBehaviour;
public class WeepingVinesPlantBlock extends GrowingPlantBodyBlock {
    public WeepingVinesPlantBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null, (net.minecraft.core.Direction) null, (net.minecraft.world.phys.shapes.VoxelShape) null, false);
    }
    protected GrowingPlantHeadBlock getHeadBlock() {
        return (GrowingPlantHeadBlock) Blocks.WEEPING_VINES;
    }
}

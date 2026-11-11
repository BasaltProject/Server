// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.block.state.BlockBehaviour;
public class StainedGlassBlock extends TransparentBlock implements BeaconBeamBlock {
    public StainedGlassBlock(DyeColor color, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public DyeColor getColor() {
        return DyeColor.byName(blockKey().getPath().replace("_stained_glass", ""), DyeColor.WHITE);
    }
}

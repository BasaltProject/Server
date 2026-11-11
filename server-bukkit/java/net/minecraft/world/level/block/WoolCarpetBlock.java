// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.block.state.BlockBehaviour;
public class WoolCarpetBlock extends CarpetBlock {
    protected WoolCarpetBlock(DyeColor color, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public DyeColor getColor() {
        return DyeColor.byName(blockKey().getPath().replace("_carpet", ""), DyeColor.WHITE);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.block.state.BlockState;

public abstract class AbstractBannerBlock extends BaseEntityBlock {
    protected AbstractBannerBlock(int index) {
        super(index);
    }

    public boolean isPossibleToRespawnInThis(BlockState state) {
        return true;
    }

    public DyeColor getColor() {
        String path = blockKey().getPath();
        return DyeColor.byName(path.substring(0, path.indexOf("_banner") >= 0 ? path.indexOf("_banner") : path.length())
            .replace("_wall", ""), DyeColor.WHITE);
    }
}

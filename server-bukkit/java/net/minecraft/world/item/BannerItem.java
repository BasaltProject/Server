// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.Direction;
import net.minecraft.world.level.block.Block;

public class BannerItem extends StandingAndWallBlockItem {
    public BannerItem(Block block, Block wallBlock, Item.Properties properties) {
        super(block, wallBlock, Direction.DOWN, properties);
    }

    public DyeColor getColor() {
        String path = net.minecraft.core.registries.BuiltInRegistries.ITEM.getKey(this).getPath();
        for (DyeColor color : DyeColor.values()) {
            if (path.equals(color.getSerializedName() + "_banner")) {
                return color;
            }
        }
        return DyeColor.WHITE;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.block.state.properties.BlockSetType;

final class BlockSetTypes {
    private BlockSetTypes() {}

    static BlockSetType of(Block block, String suffix) {
        String path = BuiltInRegistries.BLOCK.getKey(block).getPath();
        if (path.contains("copper")) {
            return BlockSetType.COPPER;
        }
        String name = path.endsWith(suffix) ? path.substring(0, path.length() - suffix.length()) : path;
        return BlockSetType.values().filter(type -> type.name().equals(name)).findFirst().orElse(BlockSetType.OAK);
    }
}

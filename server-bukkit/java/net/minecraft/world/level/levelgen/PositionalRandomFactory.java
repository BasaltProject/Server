// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.core.BlockPos;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;

public interface PositionalRandomFactory {
    default RandomSource at(BlockPos pos) {
        return at(pos.getX(), pos.getY(), pos.getZ());
    }

    default RandomSource fromHashOf(Identifier name) {
        return fromHashOf(name.toString());
    }

    RandomSource fromHashOf(String name);

    RandomSource fromSeed(long seed);

    RandomSource at(int x, int y, int z);

    void parityConfigString(StringBuilder sb);
}

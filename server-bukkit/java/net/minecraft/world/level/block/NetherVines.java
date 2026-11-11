// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public class NetherVines {
    public static final double GROW_PER_TICK_PROBABILITY = 0.1;
    public static boolean isValidGrowthState(BlockState state) {
        return state.isAir();
    }
    public static int getBlocksToGrowWhenBonemealed(RandomSource random) {
        double chance = 1.0;
        int count = 0;
        while (random.nextDouble() < chance) {
            chance *= 0.826;
            count++;
        }
        return count;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.BlockGetter;
public class SpreadPlayersCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "spreadplayers");
    }
    static class Position {
        private double x;
        private double z;

        public double dist(Position target) {
            double dx = x - target.x;
            double dz = z - target.z;
            return Math.sqrt(dx * dx + dz * dz);
        }
        public void normalize() {
            double length = getLength();
            x /= length;
            z /= length;
        }
        public double getLength() {
            return Math.sqrt(x * x + z * z);
        }
        public void moveAway(Position pos) {
            x -= pos.x;
            z -= pos.z;
        }
        public boolean clamp(double minX, double minZ, double maxX, double maxZ) {
            double nx = Math.min(Math.max(x, minX), maxX);
            double nz = Math.min(Math.max(z, minZ), maxZ);
            boolean changed = nx != x || nz != z;
            x = nx;
            z = nz;
            return changed;
        }
        public int getSpawnY(BlockGetter level, int maxHeight) {
            net.minecraft.core.BlockPos.MutableBlockPos pos = new net.minecraft.core.BlockPos.MutableBlockPos(x, maxHeight + 1, z);
            boolean airTwoUp = level.getBlockState(pos).isAir();
            boolean airOneUp = level.getBlockState(pos.move(net.minecraft.core.Direction.DOWN)).isAir();
            while (pos.getY() > level.getMinY()) {
                boolean here = level.getBlockState(pos.move(net.minecraft.core.Direction.DOWN)).isAir();
                if (!here && airOneUp && airTwoUp) {
                    return pos.getY() + 1;
                }
                airTwoUp = airOneUp;
                airOneUp = here;
            }
            return maxHeight + 1;
        }
        public boolean isSafe(BlockGetter level, int maxHeight) {
            net.minecraft.core.BlockPos at = net.minecraft.core.BlockPos.containing(x, getSpawnY(level, maxHeight) - 1, z);
            net.minecraft.world.level.block.state.BlockState state = level.getBlockState(at);
            return at.getY() < maxHeight && !state.liquid() && state.is(net.minecraft.tags.BlockTags.ENTITIES_CAN_TELEPORT_TO);
        }
        public void randomize(RandomSource random, double minX, double minZ, double maxX, double maxZ) {
            x = net.minecraft.util.Mth.nextDouble(random, minX, maxX);
            z = net.minecraft.util.Mth.nextDouble(random, minZ, maxZ);
        }
    }
}

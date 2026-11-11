// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.blocks.BlockInput;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
public class FillCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "fill");
    }
    static enum Mode {
        REPLACE(Affector.NOOP, Filter.NOOP),
        OUTLINE(Affector.NOOP, (r, p, b, l) -> onShell(r, p) ? b : null),
        HOLLOW(Affector.NOOP, (r, p, b, l) -> onShell(r, p) ? b : new BlockInput(
            net.minecraft.world.level.block.Blocks.AIR.defaultBlockState(), java.util.Collections.emptySet(), null)),
        DESTROY((l, p) -> l.destroyBlock(p, true, null, 512), Filter.NOOP);

        public final Filter filter;
        public final Affector affector;

        private Mode(Affector affector, Filter filter) {
            this.affector = affector;
            this.filter = filter;
        }

        private static boolean onShell(BoundingBox r, BlockPos p) {
            return p.getX() == r.minX() || p.getX() == r.maxX() || p.getY() == r.minY() || p.getY() == r.maxY()
                || p.getZ() == r.minZ() || p.getZ() == r.maxZ();
        }
    }
    public interface Filter {
        Filter NOOP = (r, p, b, l) -> b;
        BlockInput filter(BoundingBox region, BlockPos pos, BlockInput block, ServerLevel level);
    }
    public interface Affector {
        Affector NOOP = (l, p) -> false;
        boolean affect(ServerLevel level, BlockPos pos);
    }
    interface NullableCommandFunction<T, R> {
        R apply(T t) throws CommandSyntaxException;
    }
}

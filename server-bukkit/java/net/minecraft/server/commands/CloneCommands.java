// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import java.util.function.Predicate;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
public class CloneCommands {
    public static final Predicate<BlockInWorld> FILTER_AIR = block -> !block.getState().isAir();
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "clone");
    }
    static record DimensionAndPosition(ServerLevel dimension, BlockPos position) {
    }
    static enum Mode {
        FORCE, MOVE, NORMAL;
        public boolean canOverlap() {
            return this != NORMAL;
        }
    }
    static record CloneBlockEntityInfo(CompoundTag tag, DataComponentMap components) {
    }
    static record CloneBlockInfo(BlockPos pos, BlockState state, CloneBlockEntityInfo blockEntityInfo, BlockState previousStateAtDestination) {
    }
}

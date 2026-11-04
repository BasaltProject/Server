// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.ticks.LevelTickAccess;
import net.minecraft.world.ticks.ScheduledTick;
import net.minecraft.world.ticks.TickPriority;
public interface ScheduledTickAccess {
    default <T> ScheduledTick<T> createTick(BlockPos pos, T type, int tickDelay, TickPriority priority) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#createTick");
    }
    default <T> ScheduledTick<T> createTick(BlockPos pos, T type, int tickDelay) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#createTick");
    }
    default LevelTickAccess<Block> getBlockTicks() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#getBlockTicks");
    }
    default void scheduleTick(BlockPos pos, Block type, int tickDelay, TickPriority priority) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#scheduleTick");
    }
    default void scheduleTick(BlockPos pos, Block type, int tickDelay) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#scheduleTick");
    }
    default LevelTickAccess<Fluid> getFluidTicks() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#getFluidTicks");
    }
    default void scheduleTick(BlockPos pos, Fluid type, int tickDelay, TickPriority priority) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#scheduleTick");
    }
    default void scheduleTick(BlockPos pos, Fluid type, int tickDelay) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ScheduledTickAccess#scheduleTick");
    }
}

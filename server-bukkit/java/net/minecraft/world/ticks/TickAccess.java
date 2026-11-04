// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;

import net.minecraft.core.BlockPos;
public interface TickAccess<T> {
    default void schedule(ScheduledTick<T> tick) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.ticks.TickAccess#schedule");
    }
    default boolean hasScheduledTick(BlockPos pos, T type) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.ticks.TickAccess#hasScheduledTick");
    }
    default int count() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.ticks.TickAccess#count");
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
public interface SignalGetter extends BlockGetter {
    Direction[] DIRECTIONS = null;
    default int getDirectSignal(BlockPos pos, Direction direction) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#getDirectSignal");
    }
    default int getDirectSignalTo(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#getDirectSignalTo");
    }
    default int getControlInputSignal(BlockPos pos, Direction direction, boolean onlyDiodes) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#getControlInputSignal");
    }
    default boolean hasSignal(BlockPos pos, Direction direction) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#hasSignal");
    }
    default int getSignal(BlockPos pos, Direction direction) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#getSignal");
    }
    default int getBestOwnOrNeighbourSignal(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#getBestOwnOrNeighbourSignal");
    }
    default boolean hasNeighborSignal(BlockPos blockPos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#hasNeighborSignal");
    }
    default int getBestNeighborSignal(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.SignalGetter#getBestNeighborSignal");
    }
}

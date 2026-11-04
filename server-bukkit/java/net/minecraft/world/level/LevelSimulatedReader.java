// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.material.FluidState;
public interface LevelSimulatedReader {
    default boolean isStateAtPosition(BlockPos pos, Predicate<BlockState> predicate) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelSimulatedReader#isStateAtPosition");
    }
    default boolean isFluidAtPosition(BlockPos pos, Predicate<FluidState> predicate) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelSimulatedReader#isFluidAtPosition");
    }
    default <T extends BlockEntity> Optional<T> getBlockEntity(BlockPos pos, BlockEntityType<T> type) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelSimulatedReader#getBlockEntity");
    }
    default BlockPos getHeightmapPos(Heightmap.Types type, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelSimulatedReader#getHeightmapPos");
    }
}

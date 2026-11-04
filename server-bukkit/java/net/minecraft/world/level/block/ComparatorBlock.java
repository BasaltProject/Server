// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.ComparatorMode;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import org.basaltmc.bukkit.WorldNatives;

public class ComparatorBlock extends DiodeBlock {
    public static final EnumProperty<ComparatorMode> MODE = BlockStateProperties.COMPARATOR_MODE;

    protected ComparatorBlock(int index) {
        super(index);
    }

    @Override
    protected int getDelay(BlockState state) {
        return 2;
    }

    @Override
    protected int getOutputSignal(BlockGetter level, BlockPos pos, BlockState state) {
        if (!(level instanceof net.minecraft.server.level.ServerLevel server)) {
            return 0;
        }
        byte[] raw = WorldNatives.blockEntityNbt(server.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
        net.minecraft.nbt.CompoundTag tag = raw == null ? null : org.basaltmc.bukkit.facade.NbtCodec.fromBytes(raw);
        return tag == null ? 0 : tag.getIntOr("OutputSignal", 0);
    }
}

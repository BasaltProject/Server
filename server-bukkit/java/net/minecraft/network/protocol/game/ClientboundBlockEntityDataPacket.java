package net.minecraft.network.protocol.game;

import java.util.function.BiFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.protocol.Packet;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import org.basaltmc.bukkit.Unsupported;
public class ClientboundBlockEntityDataPacket implements Packet {
    public ClientboundBlockEntityDataPacket(BlockPos blockPos, BlockEntityType blockEntityType, CompoundTag compoundTag) {
    }
    public static ClientboundBlockEntityDataPacket create(BlockEntity blockEntity, BiFunction updateTagSaver) {
        throw Unsupported.hit("net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket.create");
    }
}

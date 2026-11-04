// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.codec.ByteBufCodecs;

import java.util.function.BiFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.protocol.Packet;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import org.basaltmc.bukkit.Unsupported;
import net.minecraft.core.RegistryAccess;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.PacketType;
public class ClientboundBlockEntityDataPacket implements Packet<ClientGamePacketListener> {
    private final BlockPos pos;
    private final BlockEntityType<?> type;
    private final CompoundTag tag;
    public ClientboundBlockEntityDataPacket(BlockPos blockPos, BlockEntityType blockEntityType, CompoundTag compoundTag) {
        this.pos = blockPos;
        this.type = blockEntityType;
        this.tag = compoundTag;
    }
    public static ClientboundBlockEntityDataPacket create(BlockEntity blockEntity, BiFunction updateTagSaver) {
        return new ClientboundBlockEntityDataPacket(blockEntity.getBlockPos(), blockEntity.getType(),
            (CompoundTag) updateTagSaver.apply(blockEntity, null));
    }
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundBlockEntityDataPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundBlockEntityDataPacket.STREAM_CODEC", () -> StreamCodec.composite(BlockPos.STREAM_CODEC, ClientboundBlockEntityDataPacket::getPos, ByteBufCodecs.registry(Registries.BLOCK_ENTITY_TYPE), ClientboundBlockEntityDataPacket::getType, ByteBufCodecs.TRUSTED_COMPOUND_TAG, ClientboundBlockEntityDataPacket::getTag, ClientboundBlockEntityDataPacket::new));
    public static ClientboundBlockEntityDataPacket create(BlockEntity blockEntity) {
        return new ClientboundBlockEntityDataPacket(blockEntity.getBlockPos(), blockEntity.getType(), blockEntity.nbt());
    }
    public PacketType<ClientboundBlockEntityDataPacket> type() {
        return GamePacketTypes.CLIENTBOUND_BLOCK_ENTITY_DATA;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleBlockEntityData(this);
    }
    public BlockPos getPos() {
        return pos;
    }
    public BlockEntityType<?> getType() {
        return type;
    }
    public CompoundTag getTag() {
        return tag;
    }
}

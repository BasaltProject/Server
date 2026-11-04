// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.StructureBlockEntity;
import net.minecraft.world.level.block.state.properties.StructureMode;

public class ServerboundSetStructureBlockPacket implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundSetStructureBlockPacket> STREAM_CODEC =
        Packet.codec(ServerboundSetStructureBlockPacket::write, ServerboundSetStructureBlockPacket::new);
    private final BlockPos pos;
    private final StructureBlockEntity.UpdateType updateType;
    private final StructureMode mode;
    private final String name;
    private final BlockPos offset;
    private final Vec3i size;
    private final Mirror mirror;
    private final Rotation rotation;
    private final String data;
    private final boolean ignoreEntities;
    private final boolean strict;
    private final boolean showAir;
    private final boolean showBoundingBox;
    private final float integrity;
    private final long seed;

    public ServerboundSetStructureBlockPacket(BlockPos pos, StructureBlockEntity.UpdateType updateType, StructureMode mode, String name,
        BlockPos offset, Vec3i size, Mirror mirror, Rotation rotation, String data, boolean ignoreEntities, boolean strict,
        boolean showAir, boolean showBoundingBox, float integrity, long seed) {
        this.pos = pos;
        this.updateType = updateType;
        this.mode = mode;
        this.name = name;
        this.offset = offset;
        this.size = size;
        this.mirror = mirror;
        this.rotation = rotation;
        this.data = data;
        this.ignoreEntities = ignoreEntities;
        this.strict = strict;
        this.showAir = showAir;
        this.showBoundingBox = showBoundingBox;
        this.integrity = integrity;
        this.seed = seed;
    }

    private ServerboundSetStructureBlockPacket(FriendlyByteBuf input) {
        this.pos = input.readBlockPos();
        this.updateType = input.readEnum(StructureBlockEntity.UpdateType.class);
        this.mode = input.readEnum(StructureMode.class);
        this.name = input.readUtf();
        this.offset = new BlockPos(input.readByte(), input.readByte(), input.readByte());
        this.size = new Vec3i(input.readByte(), input.readByte(), input.readByte());
        this.mirror = input.readEnum(Mirror.class);
        this.rotation = input.readEnum(Rotation.class);
        this.data = input.readUtf();
        this.integrity = input.readFloat();
        this.seed = input.readVarLong();
        int flags = input.readByte();
        this.ignoreEntities = (flags & 1) != 0;
        this.showAir = (flags & 2) != 0;
        this.showBoundingBox = (flags & 4) != 0;
        this.strict = (flags & 8) != 0;
    }

    private void write(FriendlyByteBuf output) {
        output.writeBlockPos(pos);
        output.writeEnum(updateType);
        output.writeEnum(mode);
        output.writeUtf(name);
        output.writeByte(offset.getX());
        output.writeByte(offset.getY());
        output.writeByte(offset.getZ());
        output.writeByte(size.getX());
        output.writeByte(size.getY());
        output.writeByte(size.getZ());
        output.writeEnum(mirror);
        output.writeEnum(rotation);
        output.writeUtf(data);
        output.writeFloat(integrity);
        output.writeVarLong(seed);
        output.writeByte((ignoreEntities ? 1 : 0) | (showAir ? 2 : 0) | (showBoundingBox ? 4 : 0) | (strict ? 8 : 0));
    }

    public PacketType<ServerboundSetStructureBlockPacket> type() {
        return GamePacketTypes.SERVERBOUND_SET_STRUCTURE_BLOCK;
    }

    public void handle(ServerGamePacketListener listener) {
        listener.handleSetStructureBlock(this);
    }

    public BlockPos getPos() {
        return pos;
    }

    public StructureBlockEntity.UpdateType getUpdateType() {
        return updateType;
    }

    public StructureMode getMode() {
        return mode;
    }

    public String getName() {
        return name;
    }

    public BlockPos getOffset() {
        return offset;
    }

    public Vec3i getSize() {
        return size;
    }

    public Mirror getMirror() {
        return mirror;
    }

    public Rotation getRotation() {
        return rotation;
    }

    public String getData() {
        return data;
    }

    public boolean isIgnoreEntities() {
        return ignoreEntities;
    }

    public boolean isStrict() {
        return strict;
    }

    public boolean isShowAir() {
        return showAir;
    }

    public boolean isShowBoundingBox() {
        return showBoundingBox;
    }

    public float getIntegrity() {
        return integrity;
    }

    public long getSeed() {
        return seed;
    }
}

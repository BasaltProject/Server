// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.util.Mth;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.phys.Vec3;
public class ClientboundAddEntityPacket implements Packet<ClientGamePacketListener> {
    private final int id;
    private final UUID uuid;
    private final EntityType<?> type;
    private final double x;
    private final double y;
    private final double z;
    private final Vec3 movement;
    private final byte xRot;
    private final byte yRot;
    private final byte yHeadRot;
    private final int data;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundAddEntityPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundAddEntityPacket", () -> Packet.codec(ClientboundAddEntityPacket::write, ClientboundAddEntityPacket::new));
    public ClientboundAddEntityPacket(Entity entity, ServerEntity serverEntity) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundAddEntityPacket#<init>");
    }
    public ClientboundAddEntityPacket(Entity entity, ServerEntity serverEntity, int data) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundAddEntityPacket#<init>");
    }
    public ClientboundAddEntityPacket(Entity entity, int data, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundAddEntityPacket#<init>");
    }
    public ClientboundAddEntityPacket(int id, UUID uuid, double x, double y, double z, float xRot, float yRot, EntityType<?> type, int data, Vec3 movement, double yHeadRot) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundAddEntityPacket#<init>");
    }
    private ClientboundAddEntityPacket(RegistryFriendlyByteBuf input) {
        this.id = input.readVarInt();
        this.uuid = input.readUUID();
        this.type = ByteBufCodecs.registry(Registries.ENTITY_TYPE).decode(input);
        this.x = input.readDouble();
        this.y = input.readDouble();
        this.z = input.readDouble();
        this.movement = Vec3.LP_STREAM_CODEC.decode(input);
        this.xRot = input.readByte();
        this.yRot = input.readByte();
        this.yHeadRot = input.readByte();
        this.data = input.readVarInt();
    }
    private void write(RegistryFriendlyByteBuf output) {
        output.writeVarInt(this.id);
        output.writeUUID(this.uuid);
        ByteBufCodecs.registry(Registries.ENTITY_TYPE).encode(output, this.type);
        output.writeDouble(this.x);
        output.writeDouble(this.y);
        output.writeDouble(this.z);
        Vec3.LP_STREAM_CODEC.encode(output, this.movement);
        output.writeByte(this.xRot);
        output.writeByte(this.yRot);
        output.writeByte(this.yHeadRot);
        output.writeVarInt(this.data);
    }
    public PacketType<ClientboundAddEntityPacket> type() {
        return GamePacketTypes.CLIENTBOUND_ADD_ENTITY;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleAddEntity(this);
    }
    public int getId() {
        return id;
    }
    public UUID getUUID() {
        return uuid;
    }
    public EntityType<?> getType() {
        return type;
    }
    public double getX() {
        return x;
    }
    public double getY() {
        return y;
    }
    public double getZ() {
        return z;
    }
    public Vec3 getMovement() {
        return movement;
    }
    public float getXRot() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundAddEntityPacket#getXRot");
    }
    public float getYRot() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundAddEntityPacket#getYRot");
    }
    public float getYHeadRot() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundAddEntityPacket#getYHeadRot");
    }
    public int getData() {
        return data;
    }
}

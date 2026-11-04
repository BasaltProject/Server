// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;

public abstract class ClientboundMoveEntityPacket implements MovementPacket<ClientGamePacketListener> {
    protected final int entityId;
    protected final VecDelta delta;
    protected final byte yRot;
    protected final byte xRot;
    protected final boolean onGround;
    protected final boolean hasRot;
    protected final boolean hasPos;

    protected static boolean unpackOnGround(int properties) {
        return (properties & 1) != 0;
    }

    protected static int unpackStepCount(int properties) {
        return properties >>> 1;
    }

    protected static int packProperties(boolean onGround, int stepCount) {
        return (onGround ? 1 : 0) | stepCount << 1;
    }

    protected ClientboundMoveEntityPacket(int entityId, VecDelta delta, byte yRot, byte xRot, boolean onGround, boolean hasRot, boolean hasPos) {
        this.entityId = entityId;
        this.delta = delta;
        this.yRot = yRot;
        this.xRot = xRot;
        this.onGround = onGround;
        this.hasRot = hasRot;
        this.hasPos = hasPos;
    }

    public abstract PacketType<? extends ClientboundMoveEntityPacket> type();

    public void handle(ClientGamePacketListener listener) {
        listener.handleMoveEntity(this);
    }

    public String toString() {
        return "Entity_" + super.toString();
    }

    public Entity getEntity(Level level) {
        return level.getEntity(entityId);
    }

    public VecDelta getPositionDelta() {
        return delta;
    }

    public float getYRot() {
        return Mth.unpackDegrees(yRot);
    }

    public float getXRot() {
        return Mth.unpackDegrees(xRot);
    }

    public boolean hasRotation() {
        return hasRot;
    }

    public boolean hasPosition() {
        return hasPos;
    }

    public boolean isOnGround() {
        return onGround;
    }

    public static class PosRot extends ClientboundMoveEntityPacket {
        public static final StreamCodec<FriendlyByteBuf, PosRot> STREAM_CODEC = Packet.codec(PosRot::write, PosRot::read);

        public PosRot(int id, VecDelta delta, byte yRot, byte xRot, boolean onGround) {
            super(id, delta, yRot, xRot, onGround, true, true);
        }

        private static PosRot read(FriendlyByteBuf input) {
            int id = input.readVarInt();
            int properties = input.readVarInt();
            VecDelta delta = VecDelta.read(input, unpackStepCount(properties));
            return new PosRot(id, delta, input.readByte(), input.readByte(), unpackOnGround(properties));
        }

        private void write(FriendlyByteBuf output) {
            output.writeVarInt(entityId);
            output.writeVarInt(packProperties(onGround, delta.stepCount()));
            VecDelta.write(output, delta);
            output.writeByte(yRot);
            output.writeByte(xRot);
        }

        public PacketType<PosRot> type() {
            return GamePacketTypes.CLIENTBOUND_MOVE_ENTITY_POS_ROT;
        }
    }

    public static class Pos extends ClientboundMoveEntityPacket {
        public static final StreamCodec<FriendlyByteBuf, Pos> STREAM_CODEC = Packet.codec(Pos::write, Pos::read);

        public Pos(int id, VecDelta delta, boolean onGround) {
            super(id, delta, (byte) 0, (byte) 0, onGround, false, true);
        }

        private static Pos read(FriendlyByteBuf input) {
            int id = input.readVarInt();
            int properties = input.readVarInt();
            return new Pos(id, VecDelta.read(input, unpackStepCount(properties)), unpackOnGround(properties));
        }

        private void write(FriendlyByteBuf output) {
            output.writeVarInt(entityId);
            output.writeVarInt(packProperties(onGround, delta.stepCount()));
            VecDelta.write(output, delta);
        }

        public PacketType<Pos> type() {
            return GamePacketTypes.CLIENTBOUND_MOVE_ENTITY_POS;
        }
    }

    public static class Rot extends ClientboundMoveEntityPacket {
        public static final StreamCodec<FriendlyByteBuf, Rot> STREAM_CODEC = Packet.codec(Rot::write, Rot::read);

        public Rot(int id, byte yRot, byte xRot, boolean onGround) {
            super(id, VecDelta.ZERO, yRot, xRot, onGround, true, false);
        }

        private static Rot read(FriendlyByteBuf input) {
            int id = input.readVarInt();
            boolean onGround = input.readBoolean();
            return new Rot(id, input.readByte(), input.readByte(), onGround);
        }

        private void write(FriendlyByteBuf output) {
            output.writeVarInt(entityId);
            output.writeBoolean(onGround);
            output.writeByte(yRot);
            output.writeByte(xRot);
        }

        public PacketType<Rot> type() {
            return GamePacketTypes.CLIENTBOUND_MOVE_ENTITY_ROT;
        }
    }
}

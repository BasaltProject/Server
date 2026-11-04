// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.phys.Vec3;

public abstract class ServerboundMovePlayerPacket implements Packet<ServerGamePacketListener> {
    protected final double x;
    protected final double y;
    protected final double z;
    protected final float yRot;
    protected final float xRot;
    protected final boolean onGround;
    protected final boolean horizontalCollision;
    protected final boolean hasPos;
    protected final boolean hasRot;

    private static int packFlags(boolean onGround, boolean horizontalCollision) {
        return (onGround ? 1 : 0) | (horizontalCollision ? 2 : 0);
    }

    protected ServerboundMovePlayerPacket(double x, double y, double z, float yRot, float xRot, boolean onGround,
        boolean horizontalCollision, boolean hasPos, boolean hasRot) {
        this.x = x;
        this.y = y;
        this.z = z;
        this.yRot = yRot;
        this.xRot = xRot;
        this.onGround = onGround;
        this.horizontalCollision = horizontalCollision;
        this.hasPos = hasPos;
        this.hasRot = hasRot;
    }

    public abstract PacketType<? extends ServerboundMovePlayerPacket> type();

    public void handle(ServerGamePacketListener listener) {
        listener.handleMovePlayer(this);
    }

    public double getX(double fallback) {
        return hasPos ? x : fallback;
    }

    public double getY(double fallback) {
        return hasPos ? y : fallback;
    }

    public double getZ(double fallback) {
        return hasPos ? z : fallback;
    }

    public float getYRot(float fallback) {
        return hasRot ? yRot : fallback;
    }

    public float getXRot(float fallback) {
        return hasRot ? xRot : fallback;
    }

    public boolean isOnGround() {
        return onGround;
    }

    public boolean horizontalCollision() {
        return horizontalCollision;
    }

    public boolean hasPosition() {
        return hasPos;
    }

    public boolean hasRotation() {
        return hasRot;
    }

    public static class PosRot extends ServerboundMovePlayerPacket {
        public static final StreamCodec<FriendlyByteBuf, PosRot> STREAM_CODEC = Packet.codec(PosRot::write, PosRot::read);

        public PosRot(Vec3 pos, float yRot, float xRot, boolean onGround, boolean horizontalCollision) {
            super(pos.x, pos.y, pos.z, yRot, xRot, onGround, horizontalCollision, true, true);
        }

        public PosRot(double x, double y, double z, float yRot, float xRot, boolean onGround, boolean horizontalCollision) {
            super(x, y, z, yRot, xRot, onGround, horizontalCollision, true, true);
        }

        private static PosRot read(FriendlyByteBuf input) {
            double x = input.readDouble();
            double y = input.readDouble();
            double z = input.readDouble();
            float yRot = input.readFloat();
            float xRot = input.readFloat();
            short flags = input.readUnsignedByte();
            return new PosRot(x, y, z, yRot, xRot, (flags & 1) != 0, (flags & 2) != 0);
        }

        private void write(FriendlyByteBuf output) {
            output.writeDouble(x);
            output.writeDouble(y);
            output.writeDouble(z);
            output.writeFloat(yRot);
            output.writeFloat(xRot);
            output.writeByte(packFlags(onGround, horizontalCollision));
        }

        public PacketType<PosRot> type() {
            return GamePacketTypes.SERVERBOUND_MOVE_PLAYER_POS_ROT;
        }
    }

    public static class Pos extends ServerboundMovePlayerPacket {
        public static final StreamCodec<FriendlyByteBuf, Pos> STREAM_CODEC = Packet.codec(Pos::write, Pos::read);

        public Pos(Vec3 pos, boolean onGround, boolean horizontalCollision) {
            super(pos.x, pos.y, pos.z, 0.0F, 0.0F, onGround, horizontalCollision, true, false);
        }

        public Pos(double x, double y, double z, boolean onGround, boolean horizontalCollision) {
            super(x, y, z, 0.0F, 0.0F, onGround, horizontalCollision, true, false);
        }

        private static Pos read(FriendlyByteBuf input) {
            double x = input.readDouble();
            double y = input.readDouble();
            double z = input.readDouble();
            short flags = input.readUnsignedByte();
            return new Pos(x, y, z, (flags & 1) != 0, (flags & 2) != 0);
        }

        private void write(FriendlyByteBuf output) {
            output.writeDouble(x);
            output.writeDouble(y);
            output.writeDouble(z);
            output.writeByte(packFlags(onGround, horizontalCollision));
        }

        public PacketType<Pos> type() {
            return GamePacketTypes.SERVERBOUND_MOVE_PLAYER_POS;
        }
    }

    public static class Rot extends ServerboundMovePlayerPacket {
        public static final StreamCodec<FriendlyByteBuf, Rot> STREAM_CODEC = Packet.codec(Rot::write, Rot::read);

        public Rot(float yRot, float xRot, boolean onGround, boolean horizontalCollision) {
            super(0.0, 0.0, 0.0, yRot, xRot, onGround, horizontalCollision, false, true);
        }

        private static Rot read(FriendlyByteBuf input) {
            float yRot = input.readFloat();
            float xRot = input.readFloat();
            short flags = input.readUnsignedByte();
            return new Rot(yRot, xRot, (flags & 1) != 0, (flags & 2) != 0);
        }

        private void write(FriendlyByteBuf output) {
            output.writeFloat(yRot);
            output.writeFloat(xRot);
            output.writeByte(packFlags(onGround, horizontalCollision));
        }

        public PacketType<Rot> type() {
            return GamePacketTypes.SERVERBOUND_MOVE_PLAYER_ROT;
        }
    }

    public static class StatusOnly extends ServerboundMovePlayerPacket {
        public static final StreamCodec<FriendlyByteBuf, StatusOnly> STREAM_CODEC = Packet.codec(StatusOnly::write, StatusOnly::read);

        public StatusOnly(boolean onGround, boolean horizontalCollision) {
            super(0.0, 0.0, 0.0, 0.0F, 0.0F, onGround, horizontalCollision, false, false);
        }

        private static StatusOnly read(FriendlyByteBuf input) {
            short flags = input.readUnsignedByte();
            return new StatusOnly((flags & 1) != 0, (flags & 2) != 0);
        }

        private void write(FriendlyByteBuf output) {
            output.writeByte(packFlags(onGround, horizontalCollision));
        }

        public PacketType<StatusOnly> type() {
            return GamePacketTypes.SERVERBOUND_MOVE_PLAYER_STATUS_ONLY;
        }
    }
}

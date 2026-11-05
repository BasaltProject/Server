// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.waypoints;

import java.util.Optional;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.network.protocol.game.ClientboundTrackedWaypointPacket;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.phys.Vec3;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.ChunkPos;
public interface WaypointTransmitter extends Waypoint {
    int REALLY_FAR_DISTANCE = 332;

    default boolean isTransmittingWaypoint() {
        return false;
    }

    default Optional<Connection> makeWaypointConnectionWith(ServerPlayer player) {
        return Optional.empty();
    }

    default Waypoint.Icon waypointIcon() {
        return Waypoint.Icon.NULL;
    }

    static boolean doesSourceIgnoreReceiver(LivingEntity source, ServerPlayer receiver) {
        if (receiver.isSpectator()) {
            return false;
        }
        if (source.isSpectator() || source.hasIndirectPassenger(receiver)) {
            return true;
        }
        double range = Math.min(source.getAttributeValue(Attributes.WAYPOINT_TRANSMIT_RANGE),
            receiver.getAttributeValue(Attributes.WAYPOINT_RECEIVE_RANGE));
        return source.distanceTo(receiver) >= range;
    }

    static boolean isChunkVisible(ChunkPos chunkPos, ServerPlayer receiver) {
        return receiver.getChunkTrackingView().isInViewDistance(chunkPos.x(), chunkPos.z());
    }

    static boolean isReallyFar(LivingEntity source, ServerPlayer receiver) {
        return source.distanceTo(receiver) > REALLY_FAR_DISTANCE;
    }

    public interface Connection {
        void connect();

        void disconnect();

        void update();

        boolean isBroken();
    }

    public interface BlockConnection extends Connection {
        int distanceManhattan();

        default boolean isBroken() {
            return distanceManhattan() > 1;
        }
    }

    public static class EntityBlockConnection implements BlockConnection {
        private final LivingEntity source;
        private final Waypoint.Icon icon;
        private final ServerPlayer receiver;
        private BlockPos last;

        public EntityBlockConnection(LivingEntity source, Waypoint.Icon icon, ServerPlayer receiver) {
            this.source = source;
            this.icon = icon;
            this.receiver = receiver;
            this.last = source.blockPosition();
        }

        public void connect() {
            receiver.connection.send(ClientboundTrackedWaypointPacket.addWaypointPosition(source.getUUID(), icon, last));
        }

        public void disconnect() {
            receiver.connection.send(ClientboundTrackedWaypointPacket.removeWaypoint(source.getUUID()));
        }

        public void update() {
            BlockPos now = source.blockPosition();
            if (now.distManhattan(last) > 0) {
                receiver.connection.send(ClientboundTrackedWaypointPacket.updateWaypointPosition(source.getUUID(), icon, now));
                last = now;
            }
        }

        public int distanceManhattan() {
            return last.distManhattan(source.blockPosition());
        }

        public boolean isBroken() {
            return BlockConnection.super.isBroken() || doesSourceIgnoreReceiver(source, receiver);
        }
    }

    public interface ChunkConnection extends Connection {
        int distanceChessboard();

        default boolean isBroken() {
            return distanceChessboard() > 1;
        }
    }

    public static class EntityChunkConnection implements ChunkConnection {
        private final LivingEntity source;
        private final Waypoint.Icon icon;
        private final ServerPlayer receiver;
        private ChunkPos last;

        public EntityChunkConnection(LivingEntity source, Waypoint.Icon icon, ServerPlayer receiver) {
            this.source = source;
            this.icon = icon;
            this.receiver = receiver;
            this.last = source.chunkPosition();
        }

        public int distanceChessboard() {
            return last.getChessboardDistance(source.chunkPosition());
        }

        public void connect() {
            receiver.connection.send(ClientboundTrackedWaypointPacket.addWaypointChunk(source.getUUID(), icon, last));
        }

        public void disconnect() {
            receiver.connection.send(ClientboundTrackedWaypointPacket.removeWaypoint(source.getUUID()));
        }

        public void update() {
            ChunkPos now = source.chunkPosition();
            if (now.getChessboardDistance(last) > 0) {
                receiver.connection.send(ClientboundTrackedWaypointPacket.updateWaypointChunk(source.getUUID(), icon, now));
                last = now;
            }
        }

        public boolean isBroken() {
            return ChunkConnection.super.isBroken() || doesSourceIgnoreReceiver(source, receiver) || isChunkVisible(last, receiver);
        }
    }

    public static class EntityAzimuthConnection implements Connection {
        private final LivingEntity source;
        private final Waypoint.Icon icon;
        private final ServerPlayer receiver;
        private float lastAngle;

        public EntityAzimuthConnection(LivingEntity source, Waypoint.Icon icon, ServerPlayer receiver) {
            this.source = source;
            this.icon = icon;
            this.receiver = receiver;
            this.lastAngle = angle();
        }

        private float angle() {
            Vec3 away = receiver.position().subtract(source.position()).rotateClockwise90();
            return (float) Mth.atan2(away.z(), away.x());
        }

        public boolean isBroken() {
            return doesSourceIgnoreReceiver(source, receiver) || isChunkVisible(source.chunkPosition(), receiver)
                || !isReallyFar(source, receiver);
        }

        public void connect() {
            receiver.connection.send(ClientboundTrackedWaypointPacket.addWaypointAzimuth(source.getUUID(), icon, lastAngle));
        }

        public void disconnect() {
            receiver.connection.send(ClientboundTrackedWaypointPacket.removeWaypoint(source.getUUID()));
        }

        public void update() {
            float now = angle();
            if (Mth.abs(now - lastAngle) > 0.008726646F) {
                receiver.connection.send(ClientboundTrackedWaypointPacket.updateWaypointAzimuth(source.getUUID(), icon, now));
                lastAngle = now;
            }
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.waypoints;

import java.util.Optional;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.ChunkPos;
public interface WaypointTransmitter extends Waypoint {
    int REALLY_FAR_DISTANCE = 0;
    default boolean isTransmittingWaypoint() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter#isTransmittingWaypoint");
    }
    default Optional<Connection> makeWaypointConnectionWith(ServerPlayer player) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter#makeWaypointConnectionWith");
    }
    default Waypoint.Icon waypointIcon() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter#waypointIcon");
    }
    static boolean doesSourceIgnoreReceiver(LivingEntity source, ServerPlayer receiver) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter#doesSourceIgnoreReceiver");
    }
    static boolean isChunkVisible(ChunkPos chunkPos, ServerPlayer receiver) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter#isChunkVisible");
    }
    static boolean isReallyFar(LivingEntity source, ServerPlayer receiver) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter#isReallyFar");
    }
    public interface BlockConnection extends Connection {
        int distanceManhattan();
        default boolean isBroken() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.BlockConnection#isBroken");
        }
    }
    public static class EntityBlockConnection implements BlockConnection {
        public EntityBlockConnection(LivingEntity source, Waypoint.Icon icon, ServerPlayer receiver) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityBlockConnection#<init>");
        }
        public void connect() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityBlockConnection#connect");
        }
        public void disconnect() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityBlockConnection#disconnect");
        }
        public void update() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityBlockConnection#update");
        }
        public int distanceManhattan() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityBlockConnection#distanceManhattan");
        }
        public boolean isBroken() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityBlockConnection#isBroken");
        }
    }
    public interface ChunkConnection extends Connection {
        int distanceChessboard();
        default boolean isBroken() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.ChunkConnection#isBroken");
        }
    }
    public static class EntityChunkConnection implements ChunkConnection {
        public EntityChunkConnection(LivingEntity source, Waypoint.Icon icon, ServerPlayer receiver) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityChunkConnection#<init>");
        }
        public int distanceChessboard() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityChunkConnection#distanceChessboard");
        }
        public void connect() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityChunkConnection#connect");
        }
        public void disconnect() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityChunkConnection#disconnect");
        }
        public void update() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityChunkConnection#update");
        }
        public boolean isBroken() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityChunkConnection#isBroken");
        }
    }
    public static class EntityAzimuthConnection implements Connection {
        public EntityAzimuthConnection(LivingEntity source, Waypoint.Icon icon, ServerPlayer receiver) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityAzimuthConnection#<init>");
        }
        public boolean isBroken() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityAzimuthConnection#isBroken");
        }
        public void connect() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityAzimuthConnection#connect");
        }
        public void disconnect() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityAzimuthConnection#disconnect");
        }
        public void update() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.WaypointTransmitter.EntityAzimuthConnection#update");
        }
    }
    public interface Connection {
        void connect();
        void disconnect();
        void update();
        boolean isBroken();
    }
}

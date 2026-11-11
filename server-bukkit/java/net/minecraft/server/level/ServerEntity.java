// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.UpdateInterval;
import net.minecraft.world.phys.Vec3;

/** Tracks what was last announced for an entity; the native server owns the wire traffic. */
public class ServerEntity {
    public static final int FORCED_POS_UPDATE_PERIOD = 60;
    private final ServerLevel level;
    private final Entity entity;
    private final UpdateInterval updateInterval;
    private final boolean trackDelta;
    private final Synchronizer synchronizer;
    private Vec3 positionBase;
    private Vec3 sentMovement = Vec3.ZERO;
    private float sentXRot;
    private float sentYRot;
    private float sentHeadRot;
    private int tickCount;

    public ServerEntity(ServerLevel level, Entity entity, UpdateInterval updateInterval, boolean trackDelta, Synchronizer synchronizer) {
        this.level = level;
        this.entity = entity;
        this.updateInterval = updateInterval;
        this.trackDelta = trackDelta;
        this.synchronizer = synchronizer;
        this.positionBase = entity.position();
        this.sentXRot = entity.getXRot();
        this.sentYRot = entity.getYRot();
        this.sentHeadRot = entity.getYHeadRot();
        if (trackDelta) {
            this.sentMovement = entity.getDeltaMovement();
        }
    }

    public void sendChanges() {
        tickCount++;
        if (!updateInterval.test(tickCount) && tickCount % FORCED_POS_UPDATE_PERIOD != 0) {
            return;
        }
        positionBase = entity.position();
        sentXRot = entity.getXRot();
        sentYRot = entity.getYRot();
        sentHeadRot = entity.getYHeadRot();
        if (trackDelta) {
            sentMovement = entity.getDeltaMovement();
        }
    }

    public void removePairing(ServerPlayer player) {}

    public void addPairing(ServerPlayer player) {
        sendPairingData(player, packet -> player.connection.send(packet));
    }

    public void sendPairingData(ServerPlayer player, Consumer<Packet<ClientGamePacketListener>> broadcast) {
        broadcast.accept(entity.getAddEntityPacket(this));
    }

    public Vec3 getPositionBase() {
        return positionBase;
    }

    public Vec3 getLastSentMovement() {
        return sentMovement;
    }

    public float getLastSentXRot() {
        return sentXRot;
    }

    public float getLastSentYRot() {
        return sentYRot;
    }

    public float getLastSentYHeadRot() {
        return sentHeadRot;
    }

    public interface Synchronizer {
        void sendToTrackingPlayers(Packet<? super ClientGamePacketListener> packet);

        void sendToTrackingPlayersAndSelf(Packet<? super ClientGamePacketListener> packet);

        void sendToTrackingPlayersFiltered(Packet<? super ClientGamePacketListener> packet, Predicate<ServerPlayer> predicate);
    }
}

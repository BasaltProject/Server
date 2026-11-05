// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Marker extends Entity {
    public Marker(EntityType<?> type, Level level) {
        super(type, level);
    }
    public void tick() {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        throw new IllegalStateException("Markers should never be sent");
    }
    protected boolean canAddPassenger(Entity passenger) {
        return false;
    }
    protected boolean couldAcceptPassenger() {
        return false;
    }
    protected void addPassenger(Entity passenger) {
        throw new IllegalStateException("Should never addPassenger without checking couldAcceptPassenger()");
    }
    public PushReaction getPistonPushReaction() {
        return PushReaction.IGNORE_ENTITY;
    }
    public boolean isIgnoringBlockTriggers() {
        return true;
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
}

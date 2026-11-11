// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class EnderDragonPart extends Entity {
    public final EnderDragon parentMob = null;
    public final String name = null;
    public EnderDragonPart(EnderDragon parentMob, String name, float w, float h) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public boolean isPickable() {
        return super.isPickable();
    }
    public ItemStack getPickResult() {
        return parentMob.getPickResult();
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean is(Entity other) {
        return this == other || parentMob == other;
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        throw new UnsupportedOperationException();
    }
    public EntityDimensions getDimensions(Pose pose) {
        return super.getDimensions(pose);
    }
    public boolean shouldBeSaved() {
        return false;
    }
}

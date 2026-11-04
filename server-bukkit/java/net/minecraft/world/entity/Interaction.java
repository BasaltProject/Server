// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import com.mojang.serialization.Codec;
import java.util.UUID;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class Interaction extends Entity implements Attackable, Targeting {
    public Interaction(EntityType<?> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#defineSynchedData");
    }
    protected void readAdditionalSaveData(ValueInput input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#readAdditionalSaveData");
    }
    protected void addAdditionalSaveData(ValueOutput output) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#addAdditionalSaveData");
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#onSyncedDataUpdated");
    }
    public boolean canBeHitByProjectile() {
        return false;
    }
    public boolean isPickable() {
        return true;
    }
    public PushReaction getPistonPushReaction() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#getPistonPushReaction");
    }
    public boolean isIgnoringBlockTriggers() {
        return true;
    }
    public boolean skipAttackInteraction(Entity source) {
        return true;
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#hurtServer");
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#interact");
    }
    public void tick() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#tick");
    }
    public LivingEntity getLastAttacker() {
        return null;
    }
    public LivingEntity getTarget() {
        return null;
    }
    public EntityDimensions getDimensions(Pose pose) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#getDimensions");
    }
    protected AABB makeBoundingBox(Vec3 position) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Interaction#makeBoundingBox");
    }
    static record PlayerAction(UUID player, long timestamp) {
        public static final Codec<PlayerAction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.Interaction.PlayerAction.class, "CODEC");
    }
}

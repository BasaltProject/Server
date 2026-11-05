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
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public boolean canBeHitByProjectile() {
        return false;
    }
    public boolean isPickable() {
        return true;
    }
    public PushReaction getPistonPushReaction() {
        return PushReaction.IGNORE_ENTITY;
    }
    public boolean isIgnoringBlockTriggers() {
        return true;
    }
    public boolean skipAttackInteraction(Entity source) {
        return source instanceof Player && !(live() instanceof org.bukkit.entity.Interaction api && api.isResponsive());
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return InteractionResult.CONSUME;
    }
    public void tick() {
    }
    public LivingEntity getLastAttacker() {
        return playerOf(live() instanceof org.bukkit.entity.Interaction api ? api.getLastAttack() : null);
    }
    public LivingEntity getTarget() {
        return playerOf(live() instanceof org.bukkit.entity.Interaction api ? api.getLastInteraction() : null);
    }
    private Player playerOf(org.bukkit.entity.Interaction.PreviousInteraction previous) {
        return previous == null ? null : level().getPlayerByUUID(previous.getPlayer().getUniqueId());
    }
    public EntityDimensions getDimensions(Pose pose) {
        org.bukkit.entity.Interaction api = live() instanceof org.bukkit.entity.Interaction i ? i : null;
        return api == null ? EntityDimensions.scalable(1.0F, 1.0F) : EntityDimensions.fixed(api.getInteractionWidth(), api.getInteractionHeight());
    }
    protected AABB makeBoundingBox(Vec3 position) {
        return getDimensions(Pose.STANDING).makeBoundingBox(position);
    }
    static record PlayerAction(UUID player, long timestamp) {
        public static final Codec<PlayerAction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.Interaction.PlayerAction.class, "CODEC");
    }
}

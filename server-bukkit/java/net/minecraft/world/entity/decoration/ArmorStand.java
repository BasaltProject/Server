// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import net.minecraft.world.entity.ai.attributes.Attributes;
import com.mojang.serialization.Codec;
import net.minecraft.core.Rotations;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.HumanoidArm;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class ArmorStand extends LivingEntity {
    private org.bukkit.entity.ArmorStand stand() {
        return live() instanceof org.bukkit.entity.ArmorStand stand ? stand : null;
    }

    private void pose(java.util.function.BiConsumer<org.bukkit.entity.ArmorStand, org.bukkit.util.EulerAngle> setter, Rotations rotations) {
        org.bukkit.entity.ArmorStand stand = stand();
        if (stand != null) {
            setter.accept(stand, new org.bukkit.util.EulerAngle(Math.toRadians(rotations.x()), Math.toRadians(rotations.y()),
                Math.toRadians(rotations.z())));
        }
    }

    private static Rotations rotations(org.bukkit.util.EulerAngle angle) {
        return angle == null ? new Rotations(0.0F, 0.0F, 0.0F)
            : new Rotations((float) Math.toDegrees(angle.getX()), (float) Math.toDegrees(angle.getY()), (float) Math.toDegrees(angle.getZ()));
    }

    public static final int WOBBLE_TIME = 5;
    public static final Rotations DEFAULT_HEAD_POSE = null;
    public static final Rotations DEFAULT_BODY_POSE = null;
    public static final Rotations DEFAULT_LEFT_ARM_POSE = null;
    public static final Rotations DEFAULT_RIGHT_ARM_POSE = null;
    public static final Rotations DEFAULT_LEFT_LEG_POSE = null;
    public static final Rotations DEFAULT_RIGHT_LEG_POSE = null;
    public static final int DISABLE_TAKING_OFFSET = 8;
    public static final int DISABLE_PUTTING_OFFSET = 16;
    public static final int CLIENT_FLAG_SMALL = 1;
    public static final int CLIENT_FLAG_SHOW_ARMS = 4;
    public static final int CLIENT_FLAG_NO_BASEPLATE = 8;
    public static final int CLIENT_FLAG_MARKER = 16;
    public static final EntityDataAccessor<Byte> DATA_CLIENT_FLAGS = null;
    public static final EntityDataAccessor<Rotations> DATA_HEAD_POSE = null;
    public static final EntityDataAccessor<Rotations> DATA_BODY_POSE = null;
    public static final EntityDataAccessor<Rotations> DATA_LEFT_ARM_POSE = null;
    public static final EntityDataAccessor<Rotations> DATA_RIGHT_ARM_POSE = null;
    public static final EntityDataAccessor<Rotations> DATA_LEFT_LEG_POSE = null;
    public static final EntityDataAccessor<Rotations> DATA_RIGHT_LEG_POSE = null;
    public long lastHit;
    public ArmorStand(EntityType<? extends ArmorStand> type, Level level) {
        super(type, level);
    }
    public ArmorStand(Level level, double x, double y, double z) {
        super();
    }
    public static AttributeSupplier.Builder createAttributes() {
        return LivingEntity.createLivingAttributes().add(Attributes.STEP_HEIGHT, 0.0);
    }
    public boolean isEffectiveAi() {
        return false;
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return slot != EquipmentSlot.BODY && slot != EquipmentSlot.SADDLE;
    }
    public boolean isPushable() {
        return false;
    }
    protected void doPush(Entity entity) {
        return;
    }
    protected void pushEntities() {
        return;
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return true;
    }
    public void setYBodyRot(float yBodyRot) {
        setYRot(yBodyRot);
    }
    public void setYHeadRot(float yHeadRot) {
        setYRot(yHeadRot);
    }
    public boolean isBaby() {
        return isSmall();
    }
    public boolean ignoreExplosion(Explosion explosion) {
        return isMarker();
    }
    public boolean isIgnoringBlockTriggers() {
        return true;
    }
    public boolean isSmall() {
        org.bukkit.entity.ArmorStand stand = stand();
        return stand != null && stand.isSmall();
    }
    public void setShowArms(boolean value) {
        org.bukkit.entity.ArmorStand stand = stand();
        if (stand != null) {
            stand.setArms(value);
        }
    }
    public boolean showArms() {
        org.bukkit.entity.ArmorStand stand = stand();
        return stand != null && stand.hasArms();
    }
    public void setNoBasePlate(boolean value) {
        org.bukkit.entity.ArmorStand stand = stand();
        if (stand != null) {
            stand.setBasePlate(!value);
        }
    }
    public boolean showBasePlate() {
        org.bukkit.entity.ArmorStand stand = stand();
        return stand == null || stand.hasBasePlate();
    }
    public boolean isMarker() {
        org.bukkit.entity.ArmorStand stand = stand();
        return stand != null && stand.isMarker();
    }
    public void setHeadPose(Rotations headPose) {
        pose(org.bukkit.entity.ArmorStand::setHeadPose, headPose);
    }
    public void setBodyPose(Rotations bodyPose) {
        pose(org.bukkit.entity.ArmorStand::setBodyPose, bodyPose);
    }
    public void setLeftArmPose(Rotations leftArmPose) {
        pose(org.bukkit.entity.ArmorStand::setLeftArmPose, leftArmPose);
    }
    public void setRightArmPose(Rotations rightArmPose) {
        pose(org.bukkit.entity.ArmorStand::setRightArmPose, rightArmPose);
    }
    public void setLeftLegPose(Rotations leftLegPose) {
        pose(org.bukkit.entity.ArmorStand::setLeftLegPose, leftLegPose);
    }
    public void setRightLegPose(Rotations rightLegPose) {
        pose(org.bukkit.entity.ArmorStand::setRightLegPose, rightLegPose);
    }
    public Rotations getHeadPose() {
        return rotations(stand() == null ? null : stand().getHeadPose());
    }
    public Rotations getBodyPose() {
        return rotations(stand() == null ? null : stand().getBodyPose());
    }
    public Rotations getLeftArmPose() {
        return rotations(stand() == null ? null : stand().getLeftArmPose());
    }
    public Rotations getRightArmPose() {
        return rotations(stand() == null ? null : stand().getRightArmPose());
    }
    public Rotations getLeftLegPose() {
        return rotations(stand() == null ? null : stand().getLeftLegPose());
    }
    public Rotations getRightLegPose() {
        return rotations(stand() == null ? null : stand().getRightLegPose());
    }
    public boolean isPickable() {
        return super.isPickable() && !isMarker();
    }
    public boolean skipAttackInteraction(Entity source) {
        return source instanceof Player player && !level().mayInteract(player, blockPosition());
    }
    public HumanoidArm getMainArm() {
        return HumanoidArm.RIGHT;
    }
    public boolean isAffectedByPotions() {
        return false;
    }
    public boolean attackable() {
        return false;
    }
    public boolean canBeSeenByAnyone() {
        return !isInvisible() && !isMarker();
    }
    public void setArmorStandPose(ArmorStandPose pose) {
        if (live() instanceof org.bukkit.entity.ArmorStand api) {
            api.setHeadPose(radians(pose.head()));
            api.setBodyPose(radians(pose.body()));
            api.setLeftArmPose(radians(pose.leftArm()));
            api.setRightArmPose(radians(pose.rightArm()));
            api.setLeftLegPose(radians(pose.leftLeg()));
            api.setRightLegPose(radians(pose.rightLeg()));
        }
    }
    public ArmorStandPose getArmorStandPose() {
        if (!(live() instanceof org.bukkit.entity.ArmorStand api)) {
            return ArmorStandPose.DEFAULT;
        }
        return new ArmorStandPose(degrees(api.getHeadPose()), degrees(api.getBodyPose()), degrees(api.getLeftArmPose()),
            degrees(api.getRightArmPose()), degrees(api.getLeftLegPose()), degrees(api.getRightLegPose()));
    }
    public static record ArmorStandPose(Rotations head, Rotations body, Rotations leftArm, Rotations rightArm, Rotations leftLeg, Rotations rightLeg) {
        public static final ArmorStandPose DEFAULT = new ArmorStandPose(new Rotations(0, 0, 0), new Rotations(0, 0, 0),
            new Rotations(-10, 0, -10), new Rotations(-15, 0, 10), new Rotations(-1, 0, -1), new Rotations(1, 0, 1));
        public static final Codec<ArmorStandPose> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.decoration.ArmorStand.ArmorStandPose.class, "CODEC");
    }
    private static org.bukkit.util.EulerAngle radians(Rotations rotations) {
        return new org.bukkit.util.EulerAngle(Math.toRadians(rotations.x()), Math.toRadians(rotations.y()), Math.toRadians(rotations.z()));
    }
    private static Rotations degrees(org.bukkit.util.EulerAngle angle) {
        return new Rotations((float) Math.toDegrees(angle.getX()), (float) Math.toDegrees(angle.getY()), (float) Math.toDegrees(angle.getZ()));
    }
    public void travel(Vec3 input) {
        return;
    }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        return;
    }
    public Vec3 getLightProbePosition(float partialTickTime) {
        return position().add(0.0, getBbHeight() * 0.5, 0.0);
    }
}

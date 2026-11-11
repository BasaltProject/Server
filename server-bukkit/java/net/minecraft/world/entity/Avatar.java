// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import java.util.Map;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.world.entity.player.PlayerModelPart;
import net.minecraft.world.item.component.ResolvableProfile;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public abstract class Avatar extends LivingEntity {
    protected Avatar() {
    }
    public static final HumanoidArm DEFAULT_MAIN_HAND = HumanoidArm.RIGHT;
    public static final int DEFAULT_MODEL_CUSTOMIZATION = 0;
    public static final float DEFAULT_EYE_HEIGHT = 1.62F;
    public static final Vec3 DEFAULT_VEHICLE_ATTACHMENT = new Vec3(0.0, 0.6, 0.0);
    public static final float SWIMMING_BB_HEIGHT = 0.6F;
    protected static final EntityDimensions STANDING_DIMENSIONS = EntityDimensions.scalable(0.6F, 1.8F).withEyeHeight(DEFAULT_EYE_HEIGHT)
        .withAttachments(EntityAttachments.builder().attach(EntityAttachment.VEHICLE, DEFAULT_VEHICLE_ATTACHMENT));
    private static final EntityDimensions SWIM_LIKE = EntityDimensions.scalable(0.6F, SWIMMING_BB_HEIGHT).withEyeHeight(0.4F);
    protected static final Map<Pose, EntityDimensions> POSES = Map.of(Pose.STANDING, STANDING_DIMENSIONS,
        Pose.SLEEPING, SLEEPING_DIMENSIONS, Pose.FALL_FLYING, SWIM_LIKE, Pose.SWIMMING, SWIM_LIKE,
        Pose.SPIN_ATTACK, SWIM_LIKE, Pose.CROUCHING, EntityDimensions.scalable(0.6F, 1.5F).withEyeHeight(1.27F)
            .withAttachments(EntityAttachments.builder().attach(EntityAttachment.VEHICLE, DEFAULT_VEHICLE_ATTACHMENT)),
        Pose.DYING, EntityDimensions.fixed(0.2F, 0.2F).withEyeHeight(DEFAULT_EYE_HEIGHT));
    protected static final EntityDataAccessor<HumanoidArm> DATA_PLAYER_MAIN_HAND = net.minecraft.network.syncher.EntityDataSerializers.HUMANOID_ARM.createAccessor(15);
    protected static final EntityDataAccessor<Byte> DATA_PLAYER_MODE_CUSTOMISATION = net.minecraft.network.syncher.EntityDataSerializers.BYTE.createAccessor(16);
    protected Avatar(EntityType<? extends LivingEntity> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public HumanoidArm getMainArm() {
        return live() instanceof org.bukkit.entity.HumanEntity human && human.getMainHand() == org.bukkit.inventory.MainHand.LEFT
            ? HumanoidArm.LEFT : DEFAULT_MAIN_HAND;
    }
    public void setMainArm(HumanoidArm mainArm) {
    }
    public boolean isModelPartShown(PlayerModelPart part) {
        if (!(live() instanceof org.bukkit.entity.Player player)) {
            return false;
        }
        int raw = player.getClientOption(com.destroystokyo.paper.ClientOption.SKIN_PARTS).getRaw();
        return (raw & part.getMask()) == part.getMask();
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return POSES.getOrDefault(pose, STANDING_DIMENSIONS);
    }
    public ResolvableProfile getProfile() {
        return null;
    }
}

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
    public static final HumanoidArm DEFAULT_MAIN_HAND = null;
    public static final int DEFAULT_MODEL_CUSTOMIZATION = 0;
    public static final float DEFAULT_EYE_HEIGHT = 1.62F;
    public static final Vec3 DEFAULT_VEHICLE_ATTACHMENT = null;
    public static final float SWIMMING_BB_HEIGHT = 0.6F;
    protected static final EntityDimensions STANDING_DIMENSIONS = null;
    protected static final Map<Pose, EntityDimensions> POSES = null;
    protected static final EntityDataAccessor<HumanoidArm> DATA_PLAYER_MAIN_HAND = null;
    protected static final EntityDataAccessor<Byte> DATA_PLAYER_MODE_CUSTOMISATION = null;
    protected Avatar(EntityType<? extends LivingEntity> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Avatar#defineSynchedData");
    }
    public HumanoidArm getMainArm() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Avatar#getMainArm");
    }
    public void setMainArm(HumanoidArm mainArm) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Avatar#setMainArm");
    }
    public boolean isModelPartShown(PlayerModelPart part) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Avatar#isModelPartShown");
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Avatar#getDefaultDimensions");
    }
    public ResolvableProfile getProfile() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Avatar#getProfile");
    }
}

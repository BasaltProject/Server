// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import com.mojang.serialization.Codec;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.world.entity.Avatar;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.item.component.ResolvableProfile;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Mannequin extends Avatar {
    private ResolvableProfile profile;
    private Component description;
    protected static final EntityDataAccessor<ResolvableProfile> DATA_PROFILE = net.minecraft.network.syncher.EntityDataSerializers.RESOLVABLE_PROFILE.createAccessor(17);
    public static final Codec<Pose> POSE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.decoration.Mannequin.class, "POSE_CODEC");
    public static final ResolvableProfile DEFAULT_PROFILE = ResolvableProfile.Static.EMPTY;
    protected static EntityType.EntityFactory<Mannequin> constructor;
    public Mannequin(EntityType<Mannequin> type, Level level) {
        super(type, level);
    }
    protected Mannequin(Level level) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public static Mannequin create(EntityType<Mannequin> type, Level level) {
        return constructor.create(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public ResolvableProfile getProfile() {
        return profile;
    }
    protected Component getDescription() {
        return description;
    }
    protected boolean isImmobile() {
        return false;
    }
    public boolean isEffectiveAi() {
        return super.isEffectiveAi();
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
}

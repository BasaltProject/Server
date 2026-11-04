// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.component.DataComponents;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.item.alchemy.PotionContents;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class AreaEffectCloud extends Entity implements TraceableEntity {
    public static final float DEFAULT_WIDTH = 6.0F;
    public static final float HEIGHT = 0.5F;
    public static final int INFINITE_DURATION = -1;
    public static final int DEFAULT_LINGERING_DURATION = 600;
    public AreaEffectCloud(EntityType<? extends AreaEffectCloud> type, Level level) {
        super(type, level);
    }
    public AreaEffectCloud(Level level, double x, double y, double z) {
        this(EntityTypes.AREA_EFFECT_CLOUD, level);
        setPos(x, y, z);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void setRadius(float radius) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            cloud.setRadius(radius);
        }
    }
    public void refreshDimensions() {
    }
    public float getRadius() {
        return live() instanceof org.bukkit.entity.AreaEffectCloud cloud ? cloud.getRadius() : 0.5F;
    }
    public void setPotionContents(PotionContents contents) {
        this.potionContents = contents;
        for (MobEffectInstance effect : contents.getAllEffects()) {
            addEffect(effect);
        }
    }
    public void setCustomParticle(ParticleOptions customParticle) {
        this.customParticle = customParticle;
    }
    public void setPotionDurationScale(float scale) {
        this.potionDurationScale = scale;
    }
    public void addEffect(MobEffectInstance effect) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            org.bukkit.potion.PotionEffectType type = org.bukkit.potion.PotionEffectType.getByKey(
                org.bukkit.NamespacedKey.fromString(effect.getEffect().unwrapKey().orElseThrow().identifier().toString()));
            if (type != null) {
                cloud.addCustomEffect(new org.bukkit.potion.PotionEffect(type, effect.getDuration(), effect.getAmplifier(), effect.isAmbient(),
                    effect.isVisible(), effect.showIcon()), true);
            }
        }
    }
    public ParticleOptions getParticle() {
        return customParticle;
    }
    protected void setWaiting(boolean waiting) {
        this.waiting = waiting;
    }
    public boolean isWaiting() {
        return waiting;
    }
    public int getDuration() {
        return live() instanceof org.bukkit.entity.AreaEffectCloud cloud ? cloud.getDuration() : 0;
    }
    public void setDuration(int duration) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            cloud.setDuration(duration);
        }
    }
    public void tick() {
    }
    public float getRadiusOnUse() {
        return live() instanceof org.bukkit.entity.AreaEffectCloud cloud ? cloud.getRadiusOnUse() : 0.0F;
    }
    public void setRadiusOnUse(float radiusOnUse) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            cloud.setRadiusOnUse(radiusOnUse);
        }
    }
    public float getRadiusPerTick() {
        return live() instanceof org.bukkit.entity.AreaEffectCloud cloud ? cloud.getRadiusPerTick() : 0.0F;
    }
    public void setRadiusPerTick(float radiusPerTick) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            cloud.setRadiusPerTick(radiusPerTick);
        }
    }
    public int getDurationOnUse() {
        return live() instanceof org.bukkit.entity.AreaEffectCloud cloud ? cloud.getDurationOnUse() : 0;
    }
    public void setDurationOnUse(int durationOnUse) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            cloud.setDurationOnUse(durationOnUse);
        }
    }
    public int getWaitTime() {
        return live() instanceof org.bukkit.entity.AreaEffectCloud cloud ? cloud.getWaitTime() : 0;
    }
    public void setWaitTime(int waitTime) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            cloud.setWaitTime(waitTime);
        }
    }
    public void setOwner(LivingEntity owner) {
        if (live() instanceof org.bukkit.entity.AreaEffectCloud cloud) {
            cloud.setSource(owner == null || !(owner.live() instanceof org.bukkit.projectiles.ProjectileSource source) ? null : source);
        }
    }
    public LivingEntity getOwner() {
        return live() instanceof org.bukkit.entity.AreaEffectCloud cloud && cloud.getSource() instanceof org.bukkit.entity.LivingEntity source
            ? (LivingEntity) Entity.fromBukkit(source) : null;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public PushReaction getPistonPushReaction() {
        return PushReaction.IGNORE_ENTITY;
    }
    public EntityDimensions getDimensions(Pose pose) {
        return EntityDimensions.scalable(getRadius() * 2.0F, 0.5F);
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
    public <T> T get(DataComponentType<? extends T> type) {
        if (type == DataComponents.POTION_CONTENTS) {
            return castComponentValue(type, potionContents);
        }
        if (type == DataComponents.POTION_DURATION_SCALE) {
            return castComponentValue(type, potionDurationScale);
        }
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        applyImplicitComponentIfPresent(components, DataComponents.POTION_CONTENTS);
        applyImplicitComponentIfPresent(components, DataComponents.POTION_DURATION_SCALE);
        super.applyImplicitComponents(components);
    }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        if (type == DataComponents.POTION_CONTENTS) {
            setPotionContents(castComponentValue(DataComponents.POTION_CONTENTS, value));
            return true;
        }
        if (type == DataComponents.POTION_DURATION_SCALE) {
            setPotionDurationScale(castComponentValue(DataComponents.POTION_DURATION_SCALE, value));
            return true;
        }
        return super.applyImplicitComponent(type, value);
    }
    private PotionContents potionContents = PotionContents.EMPTY;
    private float potionDurationScale = 1.0F;
    private ParticleOptions customParticle;
    private boolean waiting;

}

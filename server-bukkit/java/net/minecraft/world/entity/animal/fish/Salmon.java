// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fish;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Salmon extends AbstractSchoolingFish {
    public Salmon(EntityType<? extends Salmon> type, Level level) {
        super(type, level);
    }
    public int getMaxSchoolSize() {
        return live() instanceof org.bukkit.entity.Salmon api ? (int) api.getMaxSchoolSize() : 0;
    }
    public ItemStack getBucketItemStack() {
        return new ItemStack(net.minecraft.world.item.Items.SALMON_BUCKET);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.salmon.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.salmon.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.salmon.hurt");
    }
    protected SoundEvent getFlopSound() {
        return null;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void saveToBucketTag(ItemStack bucket) {
        super.saveToBucketTag(bucket);
        net.minecraft.world.item.component.CustomData.update(net.minecraft.core.component.DataComponents.BUCKET_ENTITY_DATA, bucket,
            tag -> tag.putString("type", getVariant().getSerializedName()));
    }
    public Variant getVariant() {
        return live() instanceof org.bukkit.entity.Salmon api ? Variant.valueOf(api.getVariant().name()) : Variant.DEFAULT;
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public float getSalmonScale() {
        return getVariant().scale;
    }
    protected EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public static enum Variant implements StringRepresentable {
        SMALL(0.5F), MEDIUM(1.0F), LARGE(1.5F);
        public static final Variant DEFAULT = MEDIUM;
        private final float scale;
        Variant(float scale) {
            this.scale = scale;
        }
        public static final Codec<Variant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.fish.Salmon.Variant.class, "CODEC");
        public static final StreamCodec<ByteBuf, Variant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.fish.Salmon.Variant.class, "STREAM_CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}

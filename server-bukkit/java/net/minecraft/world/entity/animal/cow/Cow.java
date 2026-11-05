// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.cow;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Cow extends AbstractCow {
    public Cow(EntityType<? extends Cow> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public Cow getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void setVariant(Holder<CowVariant> variant) {
        if (live() instanceof org.bukkit.entity.Cow api) {
            api.setVariant((org.bukkit.entity.Cow.Variant) org.basaltmc.bukkit.facade.VariantHolders.bukkit(io.papermc.paper.registry.RegistryKey.COW_VARIANT, variant));
        }
    }
    public Holder<CowVariant> getVariant() {
        return live() instanceof org.bukkit.entity.Cow api
            ? org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.COW_VARIANT, net.minecraft.world.entity.animal.cow.CowVariant.class, api.getVariant()) : null;
    }
    protected CowSoundVariant getSoundSet() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.cow.Cow#getSoundSet");
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
}

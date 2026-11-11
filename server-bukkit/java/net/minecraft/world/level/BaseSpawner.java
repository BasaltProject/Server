// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityProcessor;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public abstract class BaseSpawner {
    public static final EntityProcessor SET_DISPLAY_ENTITY_ID = e -> {
        e.setId(-1);
        return e;
    };
    public static final String SPAWN_DATA_TAG = "SpawnData";
    private static final int kSpinEvent = 1;
    private SpawnData nextSpawnData;
    private EntityType<?> entityType;
    private double spin;
    private double oSpin;

    public void setEntityId(EntityType<?> type, Level level, RandomSource random, BlockPos pos) {
        entityType = type;
        nextSpawnData = null;
    }
    public void setEntityData(TypedEntityData<EntityType<?>> entityData, Level level, RandomSource random, BlockPos pos) {
        entityType = entityData.type();
        nextSpawnData = null;
    }
    public void clientTick(Level level, BlockPos pos) {
        oSpin = spin;
        spin = (spin + 1000.0F / 220.0F) % 360.0;
    }
    public void serverTick(ServerLevel level, BlockPos pos) {
    }
    public void load(Level level, BlockPos pos, ValueInput input) {
    }
    public void save(ValueOutput output) {
    }
    public Entity getOrCreateDisplayEntity(Level level, BlockPos pos) {
        return entityType == null ? null : entityType.create(level, net.minecraft.world.entity.EntitySpawnReason.SPAWNER);
    }
    public boolean onEventTriggered(Level level, int id) {
        if (id == kSpinEvent) {
            if (level.isClientSide()) {
                oSpin = spin;
            }
            return true;
        }
        return false;
    }
    protected void setNextSpawnData(Level level, BlockPos pos, SpawnData nextSpawnData) {
        this.nextSpawnData = nextSpawnData;
    }
    public abstract void broadcastEvent(Level level, BlockPos pos, int id);
    public double getSpin() {
        return spin;
    }
    public double getOSpin() {
        return oSpin;
    }
}

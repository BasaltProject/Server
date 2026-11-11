// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.util.Mth;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.util.RandomSource;
public class ItemBasedSteering {
    private final SynchedEntityData entityData;
    private final EntityDataAccessor<Integer> boostTimeAccessor;
    private boolean boosting;
    private int boostTime;
    public ItemBasedSteering(SynchedEntityData entityData, EntityDataAccessor<Integer> boostTimeAccessor) {
        this.entityData = entityData;
        this.boostTimeAccessor = boostTimeAccessor;
    }
    private int boostTimeTotal() {
        return entityData.get(boostTimeAccessor);
    }
    public void onSynced() {
        this.boosting = true;
        this.boostTime = 0;
    }
    public boolean boost(RandomSource random) {
        if (boosting) {
            return false;
        }
        boosting = true;
        boostTime = 0;
        entityData.set(boostTimeAccessor, random.nextInt(841) + 140);
        return true;
    }
    public void tickBoost() {
        if (boosting && boostTime++ > boostTimeTotal()) {
            boosting = false;
        }
    }
    public float boostFactor() {
        return boosting ? 1.0F + 1.15F * Mth.sin(boostTime / (float) boostTimeTotal() * (float) Math.PI) : 1.0F;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
public abstract class VehicleEntity extends Entity {
    private int hurtTime;
    private int hurtDir = 1;
    private float damage;

    protected static final EntityDataAccessor<Integer> DATA_ID_HURT = null;
    protected static final EntityDataAccessor<Integer> DATA_ID_HURTDIR = null;
    protected static final EntityDataAccessor<Float> DATA_ID_DAMAGE = null;
    public VehicleEntity(EntityType<?> type, Level level) {
        super(type, level);
    }
    public boolean hurtClient(DamageSource source) {
        return true;
    }
    protected boolean shouldSourceDestroy(DamageSource source) {
        return false;
    }
    public boolean ignoreExplosion(Explosion explosion) {
        return false;
    }
    public void destroy(ServerLevel level, Item dropItem) {
        kill(level);
        spawnAtLocation(level, new net.minecraft.world.item.ItemStack(dropItem));
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void setHurtTime(int hurtTime) {
        this.hurtTime = hurtTime;
    }
    public void setHurtDir(int hurtDir) {
        this.hurtDir = hurtDir;
    }
    public void setDamage(float damage) {
        this.damage = damage;
    }
    public float getDamage() {
        return damage;
    }
    public int getHurtTime() {
        return hurtTime;
    }
    public int getHurtDir() {
        return hurtDir;
    }
    protected void destroy(ServerLevel level, DamageSource source) {
        destroy(level, getDropItem());
    }
    public int getDimensionChangingDelay() {
        return 10;
    }
    protected abstract Item getDropItem();
}

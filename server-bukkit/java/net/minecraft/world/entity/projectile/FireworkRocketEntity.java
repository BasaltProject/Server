// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;
import net.minecraft.world.item.Items;
import it.unimi.dsi.fastutil.doubles.DoubleDoubleImmutablePair;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
public class FireworkRocketEntity extends Projectile implements ItemSupplier {
    private boolean shotAtAngleFlag;
    private ItemStack rocketItem = new ItemStack(Items.FIREWORK_ROCKET);
    public FireworkRocketEntity(EntityType<? extends FireworkRocketEntity> type, Level level) {
        super(type, level);
    }
    public FireworkRocketEntity(Level level, double x, double y, double z, ItemStack sourceItemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public FireworkRocketEntity(Level level, Entity owner, double x, double y, double z, ItemStack sourceItemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public FireworkRocketEntity(Level level, ItemStack sourceItemStack, LivingEntity stuckTo) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public FireworkRocketEntity(Level level, ItemStack sourceItemStack, double x, double y, double z, boolean shotAtAngle) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public FireworkRocketEntity(Level level, ItemStack sourceItemStack, Entity owner, double x, double y, double z, boolean shotAtAngle) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public boolean shouldRender(double camX, double camY, double camZ) {
        return super.shouldRender(camX, camY, camZ);
    }
    public void tick() {
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        }
    protected void onHitBlock(BlockHitResult hitResult) {
        }
    public boolean isShotAtAngle() {
        return shotAtAngleFlag;
    }
    public void handleEntityEvent(byte id) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public ItemStack getItem() {
        return rocketItem;
    }
    public boolean isAttackable() {
        return false;
    }
    public DoubleDoubleImmutablePair calculateHorizontalHurtKnockbackDirection(LivingEntity hurtEntity, DamageSource damageSource) {
        return DoubleDoubleImmutablePair.of(hurtEntity.position().x - position().x, hurtEntity.position().z - position().z);
    }
}

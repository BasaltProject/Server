// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.arrow;
import java.util.List;
import com.mojang.serialization.Codec;
import java.util.Collection;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractArrow extends Projectile {
    private SoundEvent soundEvent;
    private boolean inGroundFlag;
    protected int inGroundTime;
    public Pickup pickup;
    public int shakeTime;
    protected AbstractArrow(EntityType<? extends AbstractArrow> type, Level level) {
        super(type, level);
    }
    protected AbstractArrow(EntityType<? extends AbstractArrow> type, double x, double y, double z, Level level, ItemStack pickupItemStack, ItemStack firedFromWeapon) {
        super(type, level);
        setPos(x, y, z);
    }
    protected AbstractArrow(EntityType<? extends AbstractArrow> type, LivingEntity mob, Level level, ItemStack pickupItemStack, ItemStack firedFromWeapon) {
        super(type, level);
        setPos(mob.getX(), mob.getEyeY() - 0.1, mob.getZ());
        setOwner(mob);
    }
    public void setSoundEvent(SoundEvent soundEvent) {
        this.soundEvent = soundEvent;
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return true;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void lerpMotion(Vec3 movement) {
        setDeltaMovement(movement);
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public void tick() {
    }
    protected float getAirDrag() {
        return 0.99F;
    }
    protected double getDefaultGravity() {
        return 0.05;
    }
    protected boolean isInGround() {
        return inGroundFlag || live() instanceof org.bukkit.entity.AbstractArrow arrow && arrow.isInBlock();
    }
    protected void setInGround(boolean inGround) {
        this.inGroundFlag = inGround;
    }
    public boolean isPushedByFluid() {
        return false;
    }
    protected void tickDespawn() {
        return;
    }
    public void onItemBreak(ItemStack item) {
        return;
    }
    public void onAboveBubbleColumn(boolean dragDown, BlockPos pos) {
        return;
    }
    public void onInsideBubbleColumn(boolean dragDown) {
        return;
    }
    public void push(double xa, double ya, double za) {
        return;
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        Entity target = hitResult.getEntity();
        if (!(live() instanceof org.bukkit.entity.AbstractArrow arrow) || !(target.live() instanceof org.bukkit.entity.Damageable damageable)) {
            return;
        }
        double speed = getDeltaMovement().length();
        double damage = Math.min(Math.ceil(speed * arrow.getDamage()), 2.147483647E9);
        if (arrow.isCritical()) {
            damage += random.nextInt((int) (damage / 2.0) + 2);
        }
        damageable.damage(damage, arrow);
        if (target instanceof LivingEntity living) {
            doKnockback(living, null);
        }
        if (arrow.getPierceLevel() <= 0) {
            discard();
        }
    }
    protected void doKnockback(LivingEntity mob, DamageSource damageSource) {
        Vec3 push = getDeltaMovement().multiply(1.0, 0.0, 1.0).normalize();
        if (live() instanceof org.bukkit.entity.AbstractArrow arrow && push.lengthSqr() > 0.0 && arrow.getKnockbackStrength() > 0) {
            mob.push(push.x * arrow.getKnockbackStrength() * 0.6, 0.1, push.z * arrow.getKnockbackStrength() * 0.6);
        }
    }
    protected void onHitBlock(BlockHitResult hitResult) {
        this.inGroundFlag = true;
        setDeltaMovement(Vec3.ZERO);
    }
    protected void hitBlockEnchantmentEffects(ServerLevel serverLevel, BlockHitResult hitResult, ItemStack weapon) {

    }
    protected SoundEvent getDefaultHitGroundSoundEvent() {
        return net.minecraft.sounds.SoundEvents.byId("entity.arrow.hit");
    }
    protected final SoundEvent getHitGroundSoundEvent() {
        return soundEvent != null ? soundEvent : getDefaultHitGroundSoundEvent();
    }
    protected void doPostHurtEffects(LivingEntity mob) {

    }
    protected EntityHitResult findHitEntity(Vec3 from, Vec3 to) {
        if (!(live() instanceof org.bukkit.entity.AbstractArrow self)) {
            return null;
        }
        Vec3 delta = to.subtract(from);
        if (delta.lengthSqr() == 0.0) {
            return null;
        }
        org.bukkit.World world = self.getWorld();
        org.bukkit.util.RayTraceResult result = world.rayTraceEntities(new org.bukkit.Location(world, from.x, from.y, from.z),
            new org.bukkit.util.Vector(delta.x, delta.y, delta.z), delta.length(), 0.3, other -> other != self);
        if (result == null || result.getHitEntity() == null) {
            return null;
        }
        return new EntityHitResult(Entity.fromBukkit(result.getHitEntity()));
    }
    protected Collection<EntityHitResult> findHitEntities(Vec3 from, Vec3 to) {
        EntityHitResult hit = findHitEntity(from, to);
        return hit == null ? List.of() : List.of(hit);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void setOwner(Entity owner) {
        super.setOwner(owner);
    }
    public void playerTouch(Player player) {
        return;
    }
    protected boolean tryPickup(Player player) {
        if (!(live() instanceof org.bukkit.entity.AbstractArrow arrow)
            || arrow.getPickupStatus() != org.bukkit.entity.AbstractArrow.PickupStatus.ALLOWED) {
            return false;
        }
        return player.getInventory().add(getPickupItem());
    }
    protected ItemStack getPickupItem() {
        ItemStack origin = getPickupItemStackOrigin();
        return origin.isEmpty() ? getDefaultPickupItem() : origin.copy();
    }
    protected abstract ItemStack getDefaultPickupItem();
    public ItemStack getPickupItemStackOrigin() {
        return live() instanceof org.bukkit.entity.AbstractArrow arrow
            ? org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(arrow.getItemStack()) : ItemStack.EMPTY;
    }
    public void setBaseDamage(double baseDamage) {
        if (live() instanceof org.bukkit.entity.AbstractArrow arrow) {
            arrow.setDamage(baseDamage);
        }
    }
    public boolean isAttackable() {
        return false;
    }
    public void setCritArrow(boolean critArrow) {
        if (live() instanceof org.bukkit.entity.AbstractArrow arrow) {
            arrow.setCritical(critArrow);
        }
    }
    protected void setPickupItemStack(ItemStack itemStack) {
        if (live() instanceof org.bukkit.entity.AbstractArrow arrow) {
            arrow.setItemStack(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        }
    }
    public boolean isCritArrow() {
        return live() instanceof org.bukkit.entity.AbstractArrow arrow && arrow.isCritical();
    }
    public byte getPierceLevel() {
        return live() instanceof org.bukkit.entity.AbstractArrow arrow ? (byte) arrow.getPierceLevel() : 0;
    }
    public void setBaseDamageFromMob(float power) {
        int difficulty = live().getWorld().getDifficulty().ordinal();
        setBaseDamage(power * 2.0F + (difficulty * 0.11 + 0.57 * (random.nextDouble() - random.nextDouble())));
    }
    protected float getWaterInertia() {
        return 0.6F;
    }
    public void setNoPhysics(boolean noPhysics) {
        if (live() instanceof org.bukkit.entity.AbstractArrow arrow) {
            arrow.setGravity(!noPhysics);
        }
    }
    public boolean isNoPhysics() {
        return live() instanceof org.bukkit.entity.AbstractArrow arrow && !arrow.hasGravity();
    }
    public boolean isPickable() {
        return super.isPickable() && !isNoPhysics();
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    protected boolean shouldBounceOnWorldBorder() {
        return true;
    }
    public static enum Pickup {
        DISALLOWED, ALLOWED, CREATIVE_ONLY;
        public static final Codec<Pickup> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.projectile.arrow.AbstractArrow.Pickup.class, "LEGACY_CODEC");
        public static Pickup byOrdinal(int ordinal) {
            return ordinal < 0 || ordinal >= values().length ? values()[0] : values()[ordinal];
        }
    }
}

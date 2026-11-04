// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;
import net.minecraft.util.Mth;
import it.unimi.dsi.fastutil.doubles.DoubleDoubleImmutablePair;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MoveSimulationType;
import net.minecraft.world.entity.TraceableEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
public abstract class Projectile extends Entity implements TraceableEntity {
    protected EntityReference<Entity> owner;
    protected Projectile(EntityType<? extends Projectile> type, Level level) {
        super(type, level);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {

    }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    protected void setOwner(EntityReference<Entity> owner) {
        this.owner = owner;
        setOwner(owner == null ? null : owner.getEntity(level(), Entity.class));
    }
    public void setOwner(Entity owner) {
        if (live() instanceof org.bukkit.entity.Projectile projectile) {
            org.bukkit.entity.Entity shooter = owner == null ? null : owner.live();
            projectile.setShooter(shooter instanceof org.bukkit.projectiles.ProjectileSource source ? source : null);
        }
    }
    public Entity getOwner() {
        if (live() instanceof org.bukkit.entity.Projectile projectile && projectile.getShooter() instanceof org.bukkit.entity.Entity shooter) {
            return wrap(shooter);
        }
        return null;
    }
    public Entity getEffectSource() {
        return getOwner();
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected boolean ownedBy(Entity entity) {
        Entity owner = getOwner();
        return owner != null && owner.getUUID().equals(entity.getUUID());
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void restoreFrom(Entity oldEntity) {
        super.restoreFrom(oldEntity);
        if (oldEntity instanceof Projectile projectile) {
            this.owner = projectile.owner;
        }
    }
    public void tick() {
    }
    protected void checkLeftOwner() {

    }
    public Vec3 getMovementToShoot(double xd, double yd, double zd, float pow, float uncertainty) {
        Vec3 direction = new Vec3(xd, yd, zd).normalize();
        return direction.add(random.triangle(0.0, 0.0172275 * uncertainty), random.triangle(0.0, 0.0172275 * uncertainty),
            random.triangle(0.0, 0.0172275 * uncertainty)).scale(pow);
    }
    public void shoot(double xd, double yd, double zd, float pow, float uncertainty) {
        Vec3 movement = getMovementToShoot(xd, yd, zd, pow, uncertainty);
        setDeltaMovement(movement);
    }
    public void shootFromRotation(Entity source, float xRot, float yRot, float yOffset, float pow, float uncertainty) {
        float radians = (float) (Math.PI / 180.0);
        float xd = -Mth.sin(yRot * radians) * Mth.cos(xRot * radians);
        float yd = -Mth.sin((xRot + yOffset) * radians);
        float zd = Mth.cos(yRot * radians) * Mth.cos(xRot * radians);
        shoot(xd, yd, zd, pow, uncertainty);
        Vec3 known = source.getKnownMovement();
        setDeltaMovement(getDeltaMovement().add(known.x, source.onGround() ? 0.0 : known.y, known.z));
    }
    public boolean canBreakBlockInAdventureMode(ServerLevel level, BlockInWorld blockInWorld) {
        return false;
    }
    public static <T extends Projectile> T spawnProjectileFromRotation(ProjectileFactory<T> creator, ServerLevel serverLevel, ItemStack itemStack, LivingEntity source, float yOffset, float pow, float uncertainty) {
        return spawnProjectile(creator.create(serverLevel, source, itemStack), serverLevel, itemStack,
            projectile -> projectile.shootFromRotation(source, source.getXRot(), source.getYRot(), yOffset, pow, uncertainty));
    }
    public static <T extends Projectile> T spawnProjectileUsingShoot(ProjectileFactory<T> creator, ServerLevel serverLevel, ItemStack itemStack, LivingEntity source, double targetX, double targetY, double targetZ, float pow, float uncertainty) {
        return spawnProjectile(creator.create(serverLevel, source, itemStack), serverLevel, itemStack,
            projectile -> projectile.shoot(targetX, targetY, targetZ, pow, uncertainty));
    }
    public static <T extends Projectile> T spawnProjectileUsingShoot(T projectile, ServerLevel serverLevel, ItemStack itemStack, double targetX, double targetY, double targetZ, float pow, float uncertainty) {
        return spawnProjectile(projectile, serverLevel, itemStack,
            spawned -> spawned.shoot(targetX, targetY, targetZ, pow, uncertainty));
    }
    public static <T extends Projectile> T spawnProjectile(T projectile, ServerLevel serverLevel, ItemStack itemStack) {
        return spawnProjectile(projectile, serverLevel, itemStack, spawned -> {
        });
    }
    public static <T extends Projectile> T spawnProjectile(T projectile, ServerLevel serverLevel, ItemStack itemStack, Consumer<T> shootFunction) {
        shootFunction.accept(projectile);
        serverLevel.addFreshEntity(projectile);
        projectile.applyOnProjectileSpawned(serverLevel, itemStack);
        return projectile;
    }
    public void applyOnProjectileSpawned(ServerLevel serverLevel, ItemStack pickupItemStack) {

    }
    protected ProjectileDeflection hitTargetOrDeflectSelf(HitResult hitResult) {
        return ProjectileDeflection.NONE;
    }
    protected boolean shouldBounceOnWorldBorder() {
        return false;
    }
    public boolean deflect(ProjectileDeflection deflection, Entity deflectingEntity, EntityReference<Entity> newOwner, boolean byAttack, Vec3 power) {
        deflection.deflect(this, deflectingEntity, random, power);
        setOwner(newOwner);
        onDeflection(byAttack);
        return deflection != ProjectileDeflection.NONE;
    }
    public boolean deflect(ProjectileDeflection deflection, Entity deflectingEntity, EntityReference<Entity> newOwner, boolean byAttack, double power) {
        return deflect(deflection, deflectingEntity, newOwner, byAttack, new Vec3(power, power, power));
    }
    protected void onDeflection(boolean byAttack) {

    }
    protected void onItemBreak(ItemStack item) {

    }
    protected void onHit(HitResult hitResult) {

    }
    protected void onHitEntity(EntityHitResult hitResult) {

    }
    protected void onHitBlock(BlockHitResult hitResult) {

    }
    protected void onRedirectProjectile(Projectile hitProjectile) {

    }
    protected boolean canHitEntity(Entity entity) {
        return entity.canBeHitByProjectile() && !ownedBy(entity);
    }
    protected void updateRotation() {
        return;
    }
    protected static float lerpRotation(float rotO, float rot) {
        while (rot - rotO < -180.0F) {
            rotO -= 360.0F;
        }
        while (rot - rotO >= 180.0F) {
            rotO += 360.0F;
        }
        return Mth.lerp(0.2F, rotO, rot);
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        return super.getAddEntityPacket(serverEntity);
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    public boolean mayInteract(ServerLevel level, BlockPos pos) {
        return true;
    }
    public boolean mayBreak(ServerLevel level, BlockPos pos) {
        return true;
    }
    public boolean isPickable() {
        return false;
    }
    public float getPickRadius() {
        return 1.0F;
    }
    public DoubleDoubleImmutablePair calculateHorizontalHurtKnockbackDirection(LivingEntity hurtEntity, DamageSource damageSource) {
        return DoubleDoubleImmutablePair.of(getDeltaMovement().x, getDeltaMovement().z);
    }
    public int getDimensionChangingDelay() {
        return 2;
    }
    public MoveSimulationType getMoveSimulationType() {
        return MoveSimulationType.SERVER_AND_CLIENT;
    }
    public interface ProjectileFactory<T extends Projectile> {
        T create(ServerLevel level, LivingEntity entity, ItemStack itemStack);
    }
}

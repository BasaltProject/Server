// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.hurtingprojectile.windcharge;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.projectile.ItemSupplier;
import net.minecraft.world.entity.projectile.hurtingprojectile.AbstractHurtingProjectile;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.ExplosionDamageCalculator;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractWindCharge extends AbstractHurtingProjectile implements ItemSupplier {
    public static final ExplosionDamageCalculator EXPLOSION_DAMAGE_CALCULATOR = new net.minecraft.world.level.SimpleExplosionDamageCalculator(true, false, java.util.Optional.empty(),
        net.minecraft.core.registries.BuiltInRegistries.BLOCK.get(net.minecraft.tags.BlockTags.BLOCKS_WIND_CHARGE_EXPLOSIONS).map(java.util.function.Function.identity()));
    public static final double JUMP_SCALE = 0.25;
    public AbstractWindCharge(EntityType<? extends AbstractWindCharge> type, Level level) {
        super(type, level);
    }
    public AbstractWindCharge(EntityType<? extends AbstractWindCharge> type, Level level, Entity owner, double x, double y, double z) {
        super(type, x, y, z, level);
    }
    protected AbstractWindCharge(EntityType<? extends AbstractWindCharge> type, double x, double y, double z, Vec3 direction, Level level) {
        super(type, x, y, z, direction, level);
    }
    protected AABB makeBoundingBox(Vec3 position) {
        float half = getType().getDimensions().width() / 2.0F;
        float tall = getType().getDimensions().height();
        return new AABB(position.x - half, position.y - 0.15, position.z - half, position.x + half, position.y - 0.15 + tall, position.z + half);
    }
    public boolean canCollideWith(Entity entity) {
        return !(entity instanceof AbstractWindCharge) && super.canCollideWith(entity);
    }
    protected boolean canHitEntity(Entity entity) {
        return false;
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        }
    public void push(double xa, double ya, double za) {
        
    }
    protected abstract void explode(Vec3 position);
    protected void onHitBlock(BlockHitResult hitResult) {
        }
    protected void onHit(HitResult hitResult) {
        }
    protected boolean shouldBurn() {
        return false;
    }
    public ItemStack getItem() {
        return ItemStack.EMPTY;
    }
    protected float getInertia() {
        return 0;
    }
    protected float getLiquidInertia() {
        return 0;
    }
    protected ParticleOptions getTrailParticle() {
        return null;
    }
    public void tick() {
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.throwableitemprojectile;
import it.unimi.dsi.fastutil.doubles.DoubleDoubleImmutablePair;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.HitResult;
public abstract class AbstractThrownPotion extends ThrowableItemProjectile {
    public static final double SPLASH_RANGE = 4.0;
    protected static final double SPLASH_RANGE_SQ = 16.0;
    public static final Predicate<LivingEntity> WATER_SENSITIVE_OR_ON_FIRE = living -> living.isSensitiveToWater() || living.isOnFire();
    public AbstractThrownPotion(EntityType<? extends AbstractThrownPotion> type, Level level) {
        super(type, level);
    }
    public AbstractThrownPotion(EntityType<? extends AbstractThrownPotion> type, Level level, LivingEntity owner, ItemStack itemStack) {
        super(type, owner, level, itemStack);
    }
    public AbstractThrownPotion(EntityType<? extends AbstractThrownPotion> type, Level level, double x, double y, double z, ItemStack itemStack) {
        super(type, x, y, z, level, itemStack);
    }
    protected double getDefaultGravity() {
        return super.getDefaultGravity();
    }
    protected void onHitBlock(BlockHitResult hitResult) {
        }
    protected void onHit(HitResult hitResult) {
        }
    protected abstract void onHitAsPotion(ServerLevel level, ItemStack potionItem, HitResult hitResult);
    public DoubleDoubleImmutablePair calculateHorizontalHurtKnockbackDirection(LivingEntity hurtEntity, DamageSource damageSource) {
        return DoubleDoubleImmutablePair.of(hurtEntity.position().x - position().x, hurtEntity.position().z - position().z);
    }
}

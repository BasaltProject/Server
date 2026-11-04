// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.damagesource;
import net.minecraft.core.RegistryAccess;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.FireworkRocketEntity;
import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import net.minecraft.world.entity.projectile.hurtingprojectile.Fireball;
import net.minecraft.world.entity.projectile.hurtingprojectile.WitherSkull;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.phys.Vec3;
public class DamageSources {
    public DamageSources(RegistryAccess registries) {}
    private DamageSource source(net.minecraft.resources.ResourceKey<DamageType> key) {
        return new DamageSource(org.basaltmc.bukkit.facade.DamageKeys.holder(key));
    }
    private DamageSource source(net.minecraft.resources.ResourceKey<DamageType> key, Entity causing) {
        return new DamageSource(org.basaltmc.bukkit.facade.DamageKeys.holder(key), causing);
    }
    private DamageSource source(net.minecraft.resources.ResourceKey<DamageType> key, Entity direct, Entity causing) {
        return new DamageSource(org.basaltmc.bukkit.facade.DamageKeys.holder(key), direct, causing);
    }
    public DamageSource inFire() { return source(DamageTypes.IN_FIRE); }
    public DamageSource campfire() { return source(DamageTypes.CAMPFIRE); }
    public DamageSource lightningBolt() { return source(DamageTypes.LIGHTNING_BOLT); }
    public DamageSource onFire() { return source(DamageTypes.ON_FIRE); }
    public DamageSource lava() { return source(DamageTypes.LAVA); }
    public DamageSource hotFloor() { return source(DamageTypes.HOT_FLOOR); }
    public DamageSource inWall() { return source(DamageTypes.IN_WALL); }
    public DamageSource cramming() { return source(DamageTypes.CRAMMING); }
    public DamageSource drown() { return source(DamageTypes.DROWN); }
    public DamageSource starve() { return source(DamageTypes.STARVE); }
    public DamageSource cactus() { return source(DamageTypes.CACTUS); }
    public DamageSource fall() { return source(DamageTypes.FALL); }
    public DamageSource enderPearl() { return source(DamageTypes.ENDER_PEARL); }
    public DamageSource flyIntoWall() { return source(DamageTypes.FLY_INTO_WALL); }
    public DamageSource fellOutOfWorld() { return source(DamageTypes.FELL_OUT_OF_WORLD); }
    public DamageSource generic() { return source(DamageTypes.GENERIC); }
    public DamageSource magic() { return source(DamageTypes.MAGIC); }
    public DamageSource wither() { return source(DamageTypes.WITHER); }
    public DamageSource dragonBreath() { return source(DamageTypes.DRAGON_BREATH); }
    public DamageSource dryOut() { return source(DamageTypes.DRY_OUT); }
    public DamageSource sweetBerryBush() { return source(DamageTypes.SWEET_BERRY_BUSH); }
    public DamageSource freeze() { return source(DamageTypes.FREEZE); }
    public DamageSource stalagmite() { return source(DamageTypes.STALAGMITE); }
    public DamageSource outOfBorder() { return source(DamageTypes.OUTSIDE_BORDER); }
    public DamageSource genericKill() { return source(DamageTypes.GENERIC_KILL); }
    public DamageSource fallingBlock(Entity entity) { return source(DamageTypes.FALLING_BLOCK, entity); }
    public DamageSource anvil(Entity entity) { return source(DamageTypes.FALLING_ANVIL, entity); }
    public DamageSource fallingStalactite(Entity entity) { return source(DamageTypes.FALLING_STALACTITE, entity); }
    public DamageSource sting(LivingEntity mob) { return source(DamageTypes.STING, mob); }
    public DamageSource mobAttack(LivingEntity mob) { return source(DamageTypes.MOB_ATTACK, mob); }
    public DamageSource noAggroMobAttack(LivingEntity mob) { return source(DamageTypes.MOB_ATTACK_NO_AGGRO, mob); }
    public DamageSource playerAttack(Player player) { return source(DamageTypes.PLAYER_ATTACK, player); }
    public DamageSource thorns(Entity source) { return source(DamageTypes.THORNS, source); }
    public DamageSource sonicBoom(Entity entity) { return source(DamageTypes.SONIC_BOOM, entity); }
    public DamageSource mace(Entity owner) { return source(DamageTypes.MACE_SMASH, owner); }
    public DamageSource arrow(AbstractArrow arrow, Entity owner) { return source(DamageTypes.ARROW, arrow, owner); }
    public DamageSource trident(Entity trident, Entity owner) { return source(DamageTypes.TRIDENT, trident, owner); }
    public DamageSource mobProjectile(Entity entity, LivingEntity mob) { return source(DamageTypes.MOB_PROJECTILE, entity, mob); }
    public DamageSource spit(Entity entity, LivingEntity mob) { return source(DamageTypes.SPIT, entity, mob); }
    public DamageSource windCharge(Entity entity, LivingEntity mob) { return source(DamageTypes.WIND_CHARGE, entity, mob); }
    public DamageSource fireworks(FireworkRocketEntity rocket, Entity owner) { return source(DamageTypes.FIREWORKS, rocket, owner); }
    public DamageSource witherSkull(WitherSkull witherSkull, Entity owner) { return source(DamageTypes.WITHER_SKULL, witherSkull, owner); }
    public DamageSource thrown(Entity entity, Entity owner) { return source(DamageTypes.THROWN, entity, owner); }
    public DamageSource indirectMagic(Entity entity, Entity owner) { return source(DamageTypes.INDIRECT_MAGIC, entity, owner); }
    public DamageSource fireball(Fireball fireball, Entity owner) {
        return owner == null ? source(DamageTypes.UNATTRIBUTED_FIREBALL, fireball, null) : source(DamageTypes.FIREBALL, fireball, owner);
    }
    public DamageSource explosion(Entity entity, Entity cause) {
        return source(cause instanceof Player ? DamageTypes.PLAYER_EXPLOSION : DamageTypes.EXPLOSION, entity, cause);
    }
    public DamageSource explosion(Explosion explosion) {
        return explosion(explosion == null ? null : explosion.getDirectSourceEntity(), explosion == null ? null : explosion.getIndirectSourceEntity());
    }
    public DamageSource badRespawnPointExplosion(Vec3 boomPos) {
        return new DamageSource(org.basaltmc.bukkit.facade.DamageKeys.holder(DamageTypes.BAD_RESPAWN_POINT), boomPos);
    }
}

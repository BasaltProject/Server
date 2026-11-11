// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.entity.item.ItemEntity;
import org.basaltmc.bukkit.EntityNatives;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.ClipContext;
import java.util.HashMap;
import net.minecraft.util.Mth;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.phys.AABB;
public class ServerExplosion implements Explosion {
    private static final ExplosionDamageCalculator kDefaultCalculator = new ExplosionDamageCalculator();
    private final boolean fire;
    private final Explosion.BlockInteraction blockInteraction;
    private final ServerLevel level;
    private final Vec3 center;
    private final Entity source;
    private final float radius;
    private final DamageSource damageSource;
    private final ExplosionDamageCalculator damageCalculator;
    private final Map<Player, Vec3> hitPlayers = new java.util.HashMap<>();
    public ServerExplosion(ServerLevel level, Entity source, DamageSource damageSource, ExplosionDamageCalculator damageCalculator, Vec3 center, float radius, boolean fire, Explosion.BlockInteraction blockInteraction) {
        this.level = level;
        this.source = source;
        this.radius = radius;
        this.center = center;
        this.fire = fire;
        this.blockInteraction = blockInteraction;
        this.damageSource = damageSource != null ? damageSource : level.damageSources().explosion(this);
        this.damageCalculator = damageCalculator != null ? damageCalculator : source == null ? kDefaultCalculator : new EntityBasedExplosionDamageCalculator(source);
    }
    public static float getSeenPercent(Vec3 center, Entity entity) {
        AABB box = entity.getBoundingBox();
        double stepX = 1.0 / ((box.maxX - box.minX) * 2.0 + 1.0);
        double stepY = 1.0 / ((box.maxY - box.minY) * 2.0 + 1.0);
        double stepZ = 1.0 / ((box.maxZ - box.minZ) * 2.0 + 1.0);
        if (stepX < 0.0 || stepY < 0.0 || stepZ < 0.0) {
            return 0.0F;
        }
        double marginX = (1.0 - Math.floor(1.0 / stepX) * stepX) / 2.0;
        double marginZ = (1.0 - Math.floor(1.0 / stepZ) * stepZ) / 2.0;
        int clear = 0;
        int probes = 0;
        for (double fx = 0.0; fx <= 1.0; fx += stepX) {
            for (double fy = 0.0; fy <= 1.0; fy += stepY) {
                for (double fz = 0.0; fz <= 1.0; fz += stepZ) {
                    Vec3 origin = new Vec3(Mth.lerp(fx, box.minX, box.maxX) + marginX, Mth.lerp(fy, box.minY, box.maxY),
                        Mth.lerp(fz, box.minZ, box.maxZ) + marginZ);
                    ClipContext ray = new ClipContext(origin, center, ClipContext.Block.COLLIDER, ClipContext.Fluid.NONE, entity);
                    if (entity.level().clip(ray).getType() == HitResult.Type.MISS) {
                        clear++;
                    }
                    probes++;
                }
            }
        }
        return (float) clear / (float) probes;
    }
    @Override
    public float radius() {
        return radius;
    }
    @Override
    public Vec3 center() {
        return center;
    }
    public int explode() {
        boolean done = EntityNatives.explode(level.getWorld().getName(), center.x, center.y, center.z, radius,
            blockInteraction != Explosion.BlockInteraction.KEEP, source == null ? -1 : source.getId(), fire);
        return done ? 1 : 0;
    }
    public Map<Player, Vec3> getHitPlayers() {
        return hitPlayers;
    }
    @Override
    public ServerLevel level() {
        return level;
    }
    @Override
    public LivingEntity getIndirectSourceEntity() {
        return Explosion.getIndirectSourceEntity(source);
    }
    @Override
    public Entity getDirectSourceEntity() {
        return source;
    }
    public DamageSource getDamageSource() {
        return damageSource;
    }
    @Override
    public Explosion.BlockInteraction getBlockInteraction() {
        return blockInteraction;
    }
    @Override
    public boolean canTriggerBlocks() {
        if (blockInteraction != Explosion.BlockInteraction.TRIGGER_BLOCK) {
            return false;
        }
        return source == null || source.getType() != EntityTypes.BREEZE_WIND_CHARGE || level.getGameRules().get(GameRules.MOB_GRIEFING);
    }
    @Override
    public boolean shouldAffectBlocklikeEntities() {
        boolean notWind = source == null || (source.getType() != EntityTypes.BREEZE_WIND_CHARGE && source.getType() != EntityTypes.WIND_CHARGE);
        return level.getGameRules().get(GameRules.MOB_GRIEFING) ? notWind : blockInteraction.shouldAffectBlocklikeEntities() && notWind;
    }
    public boolean isSmall() {
        return radius < 2.0F || blockInteraction == Explosion.BlockInteraction.KEEP;
    }
    static class StackCollector {
        private final BlockPos pos;
        private ItemStack stack;
        private StackCollector(BlockPos pos, ItemStack stack) {
            this.pos = pos;
            this.stack = stack;
        }
        public void tryMerge(ItemStack input) {
            if (ItemEntity.areMergable(stack, input)) {
                stack = ItemEntity.merge(stack, input, 16);
            }
        }
    }
}

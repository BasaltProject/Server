// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.Predicate;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
public record AttackRange(float minReach, float maxReach, float minCreativeReach, float maxCreativeReach, float hitboxMargin, float mobFactor) {
    public static final Codec<AttackRange> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.AttackRange.class, "CODEC");
    public static final StreamCodec<ByteBuf, AttackRange> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.AttackRange.class, "STREAM_CODEC");
    public static AttackRange defaultFor(LivingEntity livingEntity) {
        float reach = (float) livingEntity.getAttributeValue(net.minecraft.world.entity.ai.attributes.Attributes.ENTITY_INTERACTION_RANGE);
        return new AttackRange(0.0F, reach, 0.0F, reach, 0.0F, 1.0F);
    }
    public HitResult getClosesetHit(Entity attacker, float partial, Predicate<Entity> matching) {
        var along = net.minecraft.world.entity.projectile.ProjectileUtil.getHitEntitiesAlong(attacker, this, matching,
            net.minecraft.world.level.ClipContext.Block.OUTLINE);
        if (along.left().isPresent()) {
            return along.left().get();
        }
        Vec3 eye = attacker.getEyePosition(partial);
        net.minecraft.world.phys.EntityHitResult closest = null;
        double best = Double.MAX_VALUE;
        for (var target : along.right().get()) {
            double distance = eye.distanceToSqr(target.getLocation());
            if (distance < best) {
                best = distance;
                closest = target;
            }
        }
        if (closest != null) {
            return closest;
        }
        Vec3 gaze = attacker.getHeadLookAngle();
        Vec3 miss = eye.add(gaze);
        return net.minecraft.world.phys.BlockHitResult.miss(miss, net.minecraft.core.Direction.getApproximateNearest(gaze),
            net.minecraft.core.BlockPos.containing(miss));
    }
    public float effectiveMinRange(Entity entity) {
        if (entity instanceof net.minecraft.world.entity.player.Player player) {
            return player.isCreative() ? minCreativeReach : minReach;
        }
        return minReach * mobFactor;
    }
    public float effectiveMaxRange(Entity entity) {
        if (entity instanceof net.minecraft.world.entity.player.Player player) {
            return player.isCreative() ? maxCreativeReach : maxReach;
        }
        return maxReach * mobFactor;
    }
    private boolean isInRange(LivingEntity attacker, java.util.function.ToDoubleFunction<Vec3> distanceSquared, double extraBuffer) {
        double distance = Math.sqrt(distanceSquared.applyAsDouble(attacker.getEyePosition()));
        return distance >= effectiveMinRange(attacker) - hitboxMargin - extraBuffer
            && distance <= effectiveMaxRange(attacker) + hitboxMargin + extraBuffer;
    }
    public boolean isInRange(LivingEntity attacker, Vec3 location) {
        return isInRange(attacker, location::distanceToSqr, 0.0);
    }
    public boolean isInRange(LivingEntity attacker, AABB boundingBox, double extraBuffer) {
        return isInRange(attacker, boundingBox::distanceToSqr, extraBuffer);
    }
}

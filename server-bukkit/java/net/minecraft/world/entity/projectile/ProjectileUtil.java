// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;

import com.mojang.datafixers.util.Either;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.function.Predicate;
import org.basaltmc.bukkit.MechanicsNatives;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import net.minecraft.world.item.ArrowItem;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.component.AttackRange;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
public final class ProjectileUtil {
    public static final float DEFAULT_ENTITY_HIT_RESULT_MARGIN = 0.3F;
    private ProjectileUtil() {
    }
    public static HitResult getHitResultOnMoveVector(Entity source, Predicate<Entity> matching) {
        return getHitResultOnMoveVector(source, matching, ClipContext.Block.COLLIDER);
    }
    public static HitResult getHitResultOnMoveVector(Entity source, Predicate<Entity> matching, ClipContext.Block clipType) {
        return sweep(source.position(), source, matching, source.getDeltaMovement(), computeMargin(source), clipType);
    }
    public static HitResult getHitResultOnViewVector(Entity source, Predicate<Entity> matching, double distance) {
        return sweep(source.getEyePosition(), source, matching, source.getViewVector(0.0F).scale(distance), 0.0F,
            ClipContext.Block.COLLIDER);
    }
    private static HitResult sweep(Vec3 from, Entity source, Predicate<Entity> matching, Vec3 delta, float margin, ClipContext.Block clipType) {
        Level level = source.level();
        Vec3 end = from.add(delta);
        HitResult best = level.clipIncludingBorder(new ClipContext(from, end, clipType, ClipContext.Fluid.NONE, source));
        if (best.getType() != HitResult.Type.MISS) {
            end = best.getLocation();
        }
        AABB area = source.getBoundingBox().expandTowards(delta).inflate(1.0);
        EntityHitResult entityHit = getEntityHitResult(level, source, from, end, area, matching, margin);
        return entityHit != null ? entityHit : best;
    }
    public static Either<BlockHitResult, Collection<EntityHitResult>> getHitEntitiesAlong(Entity attacker, AttackRange attackRange,
        Predicate<Entity> matching, ClipContext.Block blockClipType) {
        Vec3 eye = attacker.getEyePosition();
        Vec3 look = attacker.getViewVector(1.0F);
        float minimum = attackRange.effectiveMinRange(attacker);
        float maximum = attackRange.effectiveMaxRange(attacker);
        Vec3 start = eye.add(look.scale(minimum));
        Vec3 end = eye.add(look.scale(maximum));
        Level level = attacker.level();
        BlockHitResult blocked = level.clipIncludingBorder(new ClipContext(eye, end, blockClipType, ClipContext.Fluid.NONE, attacker));
        if (blocked.getType() != HitResult.Type.MISS) {
            end = blocked.getLocation();
            if (eye.distanceToSqr(end) < eye.distanceToSqr(start)) {
                return Either.left(blocked);
            }
        }
        float margin = attackRange.hitboxMargin();
        AABB area = AABB.ofSize(start, margin, margin, margin).expandTowards(end.subtract(start)).inflate(1.0);
        Collection<EntityHitResult> hits = getManyEntityHitResult(level, attacker, start, end, area, matching, margin, blockClipType, true, true);
        return hits.isEmpty() ? Either.left(blocked) : Either.right(hits);
    }
    public static EntityHitResult getEntityHitResult(Entity except, Vec3 from, Vec3 to, AABB box, Predicate<Entity> matching, double maxValue) {
        List<Entity> found = except.level().getEntities(except, box, matching);
        double[] hit = MechanicsNatives.pickedHit(candidates(found), except.getRootVehicle().getId(), from.x, from.y, from.z,
            to.x, to.y, to.z, maxValue);
        return hit == null ? null : new EntityHitResult(found.get((int) hit[0]), new Vec3(hit[1], hit[2], hit[3]));
    }
    private static double[] candidates(List<Entity> entities) {
        double[] out = new double[entities.size() * 9];
        int at = 0;
        for (Entity candidate : entities) {
            AABB box = candidate.getBoundingBox();
            out[at++] = box.minX;
            out[at++] = box.minY;
            out[at++] = box.minZ;
            out[at++] = box.maxX;
            out[at++] = box.maxY;
            out[at++] = box.maxZ;
            out[at++] = candidate.getPickRadius();
            out[at++] = candidate.canBePickedFromInside() ? 1.0 : 0.0;
            out[at++] = candidate.getRootVehicle().getId();
        }
        return out;
    }
    public static EntityHitResult getEntityHitResult(Level level, Projectile source, Vec3 from, Vec3 to, AABB targetSearchArea,
        Predicate<Entity> matching) {
        return getEntityHitResult(level, source, from, to, targetSearchArea, matching, computeMargin(source));
    }
    public static float computeMargin(Entity source) {
        return MechanicsNatives.projectileMargin(source.tickCount);
    }
    public static EntityHitResult getEntityHitResult(Level level, Entity source, Vec3 from, Vec3 to, AABB targetSearchArea,
        Predicate<Entity> matching, float entityMargin) {
        List<Entity> found = level.getEntities(source, targetSearchArea, matching);
        double[] hit = MechanicsNatives.closestHit(candidates(found), from.x, from.y, from.z, to.x, to.y, to.z, entityMargin);
        return hit == null ? null : new EntityHitResult(found.get((int) hit[0]), new Vec3(hit[1], hit[2], hit[3]));
    }
    public static Collection<EntityHitResult> getManyEntityHitResult(Level level, Entity source, Vec3 from, Vec3 to,
        AABB targetSearchArea, Predicate<Entity> matching, boolean includeFromEntity, boolean projectSurfaceHitLocation) {
        return getManyEntityHitResult(level, source, from, to, targetSearchArea, matching, computeMargin(source),
            ClipContext.Block.COLLIDER, includeFromEntity, projectSurfaceHitLocation);
    }
    public static Collection<EntityHitResult> getManyEntityHitResult(Level level, Entity source, Vec3 from, Vec3 to,
        AABB targetSearchArea, Predicate<Entity> matching, float entityMargin, ClipContext.Block clipType, boolean includeFromEntity,
        boolean projectSurfaceHitLocation) {
        List<EntityHitResult> hits = new ArrayList<>();
        for (Entity candidate : level.getEntities(source, targetSearchArea, matching)) {
            AABB box = candidate.getBoundingBox();
            if (includeFromEntity && box.contains(from)) {
                hits.add(new EntityHitResult(candidate, from));
                continue;
            }
            Optional<Vec3> direct = box.clip(from, to);
            if (direct.isPresent()) {
                hits.add(new EntityHitResult(candidate, direct.get()));
                continue;
            }
            Optional<Vec3> near = entityMargin > 0.0F ? box.inflate(entityMargin).clip(from, to) : Optional.empty();
            if (near.isEmpty()) {
                continue;
            }
            Vec3 aim = box.getCenter();
            BlockHitResult wall = level.clipIncludingBorder(new ClipContext(near.get(), aim, clipType, ClipContext.Fluid.NONE, source));
            if (wall.getType() != HitResult.Type.MISS) {
                aim = wall.getLocation();
            }
            Optional<Vec3> surface = box.clip(near.get(), aim);
            if (surface.isPresent()) {
                hits.add(new EntityHitResult(candidate, projectSurfaceHitLocation ? surface.get() : near.get()));
            }
        }
        return hits;
    }
    public static void rotateTowardsMovement(Entity projectile, float rotationSpeed) {
        Vec3 motion = projectile.getDeltaMovement();
        float[] turned = MechanicsNatives.rotateTowardsMovement(motion.x, motion.y, motion.z, projectile.xRotO, projectile.yRotO,
            projectile.getXRot(), projectile.getYRot(), rotationSpeed);
        if (turned == null) {
            return;
        }
        projectile.xRotO = turned[0];
        projectile.yRotO = turned[1];
        projectile.setXRot(turned[2]);
        projectile.setYRot(turned[3]);
    }
    public static InteractionHand getWeaponHoldingHand(LivingEntity mob, Item weaponItem) {
        return mob.getMainHandItem().is(weaponItem) ? InteractionHand.MAIN_HAND : InteractionHand.OFF_HAND;
    }
    public static AbstractArrow getMobArrow(LivingEntity mob, ItemStack projectile, float power, ItemStack firedFromWeapon) {
        ArrowItem arrowItem = projectile.getItem() instanceof ArrowItem held ? held : (ArrowItem) Items.ARROW;
        AbstractArrow arrow = arrowItem.createArrow(mob.level(), projectile, mob, firedFromWeapon);
        arrow.setBaseDamageFromMob(power);
        return arrow;
    }
}

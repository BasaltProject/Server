// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;

import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;

public interface ProjectileDeflection {
    ProjectileDeflection NONE = (projectile, entity, random, power) -> {
    };
    ProjectileDeflection REVERSE = (projectile, entity, random, power) -> {
        projectile.setDeltaMovement(projectile.getDeltaMovement().scale(-0.5));
        projectile.setYRot(projectile.getYRot() + 180.0F);
        projectile.yRotO += 180.0F;
    };
    ProjectileDeflection AIM_DEFLECT = (projectile, entity, random, power) -> {
        if (entity != null) {
            Vec3 aim = entity.getLookAngle().normalize();
            projectile.setDeltaMovement(aim);
        }
    };
    ProjectileDeflection MOMENTUM_DEFLECT = (projectile, entity, random, power) -> {
        if (entity != null) {
            projectile.setDeltaMovement(entity.getDeltaMovement().normalize());
        }
    };

    void deflect(Projectile projectile, Entity entity, RandomSource random, Vec3 power);
}

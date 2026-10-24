package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.Entity;
import org.bukkit.entity.ShulkerBullet;
import org.bukkit.util.Vector;

public interface ShulkerBulletOps extends ShulkerBullet, Cored {
    default Entity getTarget() {
        return core().entity(core().integer("bullet_target"));
    }

    default void setTarget(Entity target) {
        core().set("bullet_target", target == null ? -1 : target.getEntityId());
    }

    default Vector getTargetDelta() {
        return new Vector(core().real("bullet_delta_x"), core().real("bullet_delta_y"), core().real("bullet_delta_z"));
    }

    default void setTargetDelta(Vector delta) {
        core().set("bullet_delta_x", delta.getX());
        core().set("bullet_delta_y", delta.getY());
        core().set("bullet_delta_z", delta.getZ());
    }

    default BlockFace getCurrentMovementDirection() {
        int face = core().integer("bullet_direction");
        return face < 0 ? null : ShulkerOps.kFaces[Math.min(5, face)];
    }

    default void setCurrentMovementDirection(BlockFace direction) {
        int index = -1;
        for (int at = 0; at < ShulkerOps.kFaces.length; at++) {
            if (ShulkerOps.kFaces[at] == direction) {
                index = at;
            }
        }
        core().set("bullet_direction", index);
    }

    default int getFlightSteps() {
        return core().integer("bullet_steps");
    }

    default void setFlightSteps(int steps) {
        core().set("bullet_steps", steps);
    }
}

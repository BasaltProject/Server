package org.basaltmc.bukkit.entity;

import java.util.List;
import java.util.UUID;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.entity.ComplexLivingEntity;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.Vector;

/** One of the eight hit boxes of an ender dragon, seen from the dragon it belongs to. */
public final class BasaltDragonPart extends org.basaltmc.bukkit.gen.ComplexEntityPartBase {
    private final ComplexLivingEntity parent;
    private final EntityCore core;
    private final int index;

    public BasaltDragonPart(ComplexLivingEntity parent, EntityCore core, int index) {
        this.parent = parent;
        this.core = core;
        this.index = index;
    }

    private double[] box() {
        if (core.call("dragon_parts") instanceof String[] rows && index < rows.length) {
            String[] axes = rows[index].split(",");
            double[] box = new double[6];
            for (int at = 0; at < 6; at++) {
                box[at] = Double.parseDouble(axes[at]);
            }
            return box;
        }
        double[] here = core.numbers();
        return new double[] {here[0], here[1], here[2], here[0], here[1], here[2]};
    }

    @Override
    public ComplexLivingEntity getParent() {
        return parent;
    }

    @Override
    public int getEntityId() {
        return core.id + 1 + index;
    }

    @Override
    public UUID getUniqueId() {
        return new UUID(core.uuid.getMostSignificantBits() ^ (index + 1), core.uuid.getLeastSignificantBits());
    }

    @Override
    public EntityType getType() {
        return EntityType.UNKNOWN;
    }

    @Override
    public World getWorld() {
        return core.world;
    }

    @Override
    public Location getLocation() {
        double[] box = box();
        return new Location(core.world, (box[0] + box[3]) / 2.0, box[1], (box[2] + box[5]) / 2.0);
    }

    @Override
    public Location getLocation(Location into) {
        Location here = getLocation();
        if (into != null) {
            into.setWorld(core.world);
            into.setX(here.getX());
            into.setY(here.getY());
            into.setZ(here.getZ());
        }
        return into;
    }

    @Override
    public BoundingBox getBoundingBox() {
        double[] box = box();
        return new BoundingBox(box[0], box[1], box[2], box[3], box[4], box[5]);
    }

    @Override
    public double getHeight() {
        double[] box = box();
        return box[4] - box[1];
    }

    @Override
    public double getWidth() {
        double[] box = box();
        return box[3] - box[0];
    }

    @Override
    public Vector getVelocity() {
        return parent.getVelocity();
    }

    @Override
    public boolean isValid() {
        return parent.isValid();
    }

    @Override
    public boolean isDead() {
        return parent.isDead();
    }

    @Override
    public boolean isOnGround() {
        return parent.isOnGround();
    }

    @Override
    public int getFireTicks() {
        return parent.getFireTicks();
    }

    @Override
    public List<Entity> getPassengers() {
        return List.of();
    }

    @Override
    public Entity getVehicle() {
        return null;
    }

    @Override
    public boolean isInsideVehicle() {
        return false;
    }

    @Override
    public String getName() {
        return "EnderDragonPart";
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltDragonPart part && part.getEntityId() == getEntityId();
    }

    @Override
    public int hashCode() {
        return getEntityId();
    }
}

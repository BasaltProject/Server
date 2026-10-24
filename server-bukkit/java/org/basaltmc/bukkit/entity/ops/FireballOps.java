package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface FireballOps extends org.bukkit.entity.Fireball, Cored {
    default org.bukkit.util.Vector getAcceleration() {
        return getVelocity();
    }

    default void setAcceleration(org.bukkit.util.Vector acceleration) {
        if (acceleration == null) {
            throw new IllegalArgumentException("Vector acceleration cannot be null");
        }
        setVelocity(acceleration);
    }

    default org.bukkit.util.Vector getDirection() {
        return getAcceleration();
    }

    default void setDirection(org.bukkit.util.Vector direction) {
        if (direction == null) {
            throw new IllegalArgumentException("Vector direction cannot be null");
        }
        if (direction.isZero()) {
            setVelocity(direction);
            return;
        }
        setVelocity(direction.clone().normalize().multiply(getVelocity().length()));
    }

    default org.bukkit.util.Vector getPower() {
        return getAcceleration();
    }

    default void setPower(org.bukkit.util.Vector power) {
        setAcceleration(power);
    }
}

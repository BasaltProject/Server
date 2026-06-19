package org.basaltmc.bukkit;

import java.util.concurrent.TimeUnit;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.WorldBorder;

/** A border that belongs to no world, for the plugins that show one to a player. */
final class StandaloneBorder implements WorldBorder {
    private static final double kMaxSize = 5.999997E7;
    private static final double kMaxCenter = 2.9999984E7;

    private double size = 5.9999968E7;
    private double centerX;
    private double centerZ;
    private double damageBuffer = 5.0;
    private double damageAmount = 0.2;
    private int warningTicks = 300;
    private int warningDistance = 5;

    @Override
    public World getWorld() {
        return null;
    }

    @Override
    public void reset() {
        size = 5.9999968E7;
        centerX = 0.0;
        centerZ = 0.0;
        damageBuffer = 5.0;
        damageAmount = 0.2;
        warningTicks = 300;
        warningDistance = 5;
    }

    @Override
    public double getSize() {
        return size;
    }

    @Override
    public void setSize(double newSize) {
        size = Math.min(Math.max(newSize, 1.0), kMaxSize);
    }

    @Override
    public void changeSize(double newSize, long ticks) {
        setSize(newSize);
    }

    @Override
    public void setSize(double newSize, long seconds) {
        setSize(newSize);
    }

    @Override
    public void setSize(double newSize, TimeUnit unit, long time) {
        setSize(newSize);
    }

    @Override
    public Location getCenter() {
        return new Location(null, centerX, 0.0, centerZ);
    }

    @Override
    public void setCenter(double x, double z) {
        if (Math.abs(x) > kMaxCenter || Math.abs(z) > kMaxCenter) {
            throw new IllegalArgumentException("Border center is too far out");
        }
        centerX = x;
        centerZ = z;
    }

    @Override
    public void setCenter(Location location) {
        setCenter(location.getX(), location.getZ());
    }

    @Override
    public double getDamageBuffer() {
        return damageBuffer;
    }

    @Override
    public void setDamageBuffer(double blocks) {
        damageBuffer = blocks;
    }

    @Override
    public double getDamageAmount() {
        return damageAmount;
    }

    @Override
    public void setDamageAmount(double damage) {
        damageAmount = damage;
    }

    @Override
    public int getWarningTimeTicks() {
        return warningTicks;
    }

    @Override
    public void setWarningTimeTicks(int ticks) {
        warningTicks = ticks;
    }

    @Override
    public int getWarningDistance() {
        return warningDistance;
    }

    @Override
    public void setWarningDistance(int distance) {
        warningDistance = distance;
    }

    @Override
    public boolean isInside(Location location) {
        double half = size / 2.0;
        return location.getX() >= centerX - half && location.getX() < centerX + half
            && location.getZ() >= centerZ - half && location.getZ() < centerZ + half;
    }

    @Override
    public double getMaxSize() {
        return kMaxSize;
    }

    @Override
    public double getMaxCenterCoordinate() {
        return kMaxCenter;
    }
}

package org.basaltmc.bukkit;

import java.util.concurrent.TimeUnit;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.WorldBorder;

/** The border of one world, read from and written to the server on every call. */
public final class BasaltWorldBorder implements WorldBorder {
    private static final int kCenterX = 0;
    private static final int kCenterZ = 1;
    private static final int kSize = 2;
    private static final int kDamage = 5;
    private static final int kBuffer = 6;
    private static final int kWarnBlocks = 7;
    private static final int kWarnSeconds = 8;
    private static final double kMaxSize = 5.999997E7;
    private static final double kMaxCenter = 2.9999984E7;
    private static final double kDefaultSize = 5.9999968E7;

    private final BasaltWorld world;

    BasaltWorldBorder(BasaltWorld world) {
        this.world = world;
    }

    private double[] values() {
        double[] values = WorldEnvNatives.border(world.getName());
        if (values == null) {
            throw new IllegalStateException("The world " + world.getName() + " is not loaded");
        }
        return values;
    }

    private void apply(double[] values, double seconds) {
        WorldEnvNatives.setBorder(world.getName(), values, seconds);
    }

    @Override
    public World getWorld() {
        return world;
    }

    @Override
    public void reset() {
        double[] values = values();
        values[kCenterX] = 0.0;
        values[kCenterZ] = 0.0;
        values[kSize] = kDefaultSize;
        values[kDamage] = 0.2;
        values[kBuffer] = 5.0;
        values[kWarnBlocks] = 5;
        values[kWarnSeconds] = 15;
        apply(values, 0.0);
    }

    @Override
    public double getSize() {
        return values()[kSize];
    }

    @Override
    public void setSize(double newSize) {
        changeSize(newSize, 0L);
    }

    @Override
    public void changeSize(double newSize, long ticks) {
        double[] values = values();
        values[kSize] = Math.min(Math.max(newSize, 1.0), kMaxSize);
        apply(values, ticks / 20.0);
    }

    @Override
    public void setSize(double newSize, long seconds) {
        double[] values = values();
        values[kSize] = Math.min(Math.max(newSize, 1.0), kMaxSize);
        apply(values, seconds);
    }

    @Override
    public void setSize(double newSize, TimeUnit unit, long time) {
        setSize(newSize, unit.toSeconds(time));
    }

    @Override
    public Location getCenter() {
        double[] values = values();
        return new Location(world, values[kCenterX], 0.0, values[kCenterZ]);
    }

    @Override
    public void setCenter(double x, double z) {
        if (Math.abs(x) > kMaxCenter || Math.abs(z) > kMaxCenter) {
            throw new IllegalArgumentException("Border center is too far out");
        }
        double[] values = values();
        values[kCenterX] = x;
        values[kCenterZ] = z;
        apply(values, 0.0);
    }

    @Override
    public void setCenter(Location location) {
        setCenter(location.getX(), location.getZ());
    }

    @Override
    public double getDamageBuffer() {
        return values()[kBuffer];
    }

    @Override
    public void setDamageBuffer(double blocks) {
        double[] values = values();
        values[kBuffer] = blocks;
        apply(values, 0.0);
    }

    @Override
    public double getDamageAmount() {
        return values()[kDamage];
    }

    @Override
    public void setDamageAmount(double damage) {
        double[] values = values();
        values[kDamage] = damage;
        apply(values, 0.0);
    }

    @Override
    public int getWarningTimeTicks() {
        return (int) values()[kWarnSeconds] * 20;
    }

    @Override
    public void setWarningTimeTicks(int ticks) {
        double[] values = values();
        values[kWarnSeconds] = ticks / 20;
        apply(values, 0.0);
    }

    @Override
    public int getWarningDistance() {
        return (int) values()[kWarnBlocks];
    }

    @Override
    public void setWarningDistance(int distance) {
        double[] values = values();
        values[kWarnBlocks] = distance;
        apply(values, 0.0);
    }

    @Override
    public boolean isInside(Location location) {
        if (location.getWorld() != null && !location.getWorld().equals(world)) {
            return false;
        }
        double[] values = values();
        double half = values[kSize] / 2.0;
        return location.getX() >= values[kCenterX] - half && location.getX() < values[kCenterX] + half
            && location.getZ() >= values[kCenterZ] - half && location.getZ() < values[kCenterZ] + half;
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

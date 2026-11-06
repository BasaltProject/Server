package org.basaltmc.bukkit;

public final class MechanicsNatives {
    private MechanicsNatives() {}

    public static native float afterArmor(float damage, float armor, float toughness, float fractionAdd);
    public static native float afterProtection(float damage, float protection);
    public static native double offspringAttribute(double parentA, double parentB, double min, double max, double first, double second, double third);
    public static native boolean scalesWithDifficulty(String scaling, boolean causedByLivingNonPlayer);
    public static native float afterResistance(float damage, int amplifier);
    /** The health that is lost and the absorption that is left. */
    public static native float[] absorb(float damage, float absorption);
    /** The rotations xRotO, yRotO, xRot, yRot, or null for a projectile at rest. */
    public static native float[] rotateTowardsMovement(double mx, double my, double mz, float xRotO, float yRotO,
        float xRot, float yRot, float speed);
    /** Candidates are nine numbers each: the box, the pick radius, pickable from inside and the root vehicle id. */
    public static native double[] closestHit(double[] candidates, double fx, double fy, double fz, double tx, double ty,
        double tz, float margin);
    public static native double[] pickedHit(double[] candidates, int root, double fx, double fy, double fz, double tx,
        double ty, double tz, double maxValue);
    public static native float projectileMargin(int tickCount);
    public static native double[] shulkerBox(float size, int direction, float progressFrom, float progressTo, double x,
        double y, double z);
    public static native float angularFriction(boolean onGround, float blockFriction, boolean inLiquid);
    /** Bodies are x, y, z, width, height, yaw; the result is torque and push, or null when the leash is slack. */
    public static native double[] leashPull(double[] self, double[] holder, boolean quad, double slack,
        double[] selfMotion, double[] holderMotion, double[] knownMotion);
    /** Colliders are seven numbers each (the box and the shape it belongs to); the result is the movement that is left. */
    public static native double[] collide(double[] colliders, double[] box, double mx, double my, double mz);
    /** The stepped movement, or null when no rise beats {@code reachSquared}. */
    public static native double[] stepUp(double[] colliders, double[] grounded, double groundedOffsetY, double mx,
        double my, double mz, float maxUpStep, double reachSquared);
    /** A level based value as its prefix program (see {@code LevelBasedValue.encode}) at a level. */
    public static native String[] creativeTabItems(String tab);
    public static native byte[] regionRead(String folder, int chunkX, int chunkZ);
    public static native void regionWrite(String folder, int chunkX, int chunkZ, byte[] data, int timestamp);
    public static native int regionTimestamp(String folder, int chunkX, int chunkZ);
    public static native boolean regionFlush(String folder, boolean release);
    public static native float levelValue(double[] program, int level);
    /** The pyramid levels, then the colour and height of every beam section above the beacon. */
    public static native int[] beaconScan(String world, int x, int y, int z);
}

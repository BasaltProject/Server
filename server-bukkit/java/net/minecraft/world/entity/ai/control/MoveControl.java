// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.control;
import net.minecraft.world.entity.Mob;
public class MoveControl<T extends Mob> implements Control {
    public static final float MIN_SPEED = 0.0005F;
    public static final float MIN_SPEED_SQR = 2.5000003E-7F;
    protected static final int MAX_TURN = 90;
    protected final T mob;
    protected double wantedX;
    protected double wantedY;
    protected double wantedZ;
    protected double speedModifier;
    protected float strafeForwards;
    protected float strafeRight;
    protected Operation operation = Operation.WAIT;
    public MoveControl(T mob) {
        this.mob = mob;
    }
    public boolean hasWanted() {
        return operation == Operation.MOVE_TO;
    }
    public double getSpeedModifier() {
        return speedModifier;
    }
    public void setWantedPosition(double x, double y, double z, double speedModifier) {
        wantedX = x;
        wantedY = y;
        wantedZ = z;
        this.speedModifier = speedModifier;
        if (operation != Operation.JUMPING) {
            operation = Operation.MOVE_TO;
        }
        if (mob.live() instanceof org.bukkit.entity.Mob live) {
            live.getPathfinder().moveTo(new org.bukkit.Location(live.getWorld(), x, y, z), speedModifier);
        }
    }
    public void strafe(float forwards, float right) {
        strafeForwards = forwards;
        strafeRight = right;
        operation = Operation.STRAFE;
    }
    public void tick() {
        
    }
    protected float rotlerp(float a, float b, float max) {
        float diff = net.minecraft.util.Mth.wrapDegrees(b - a);
        return a + net.minecraft.util.Mth.clamp(diff, -max, max);
    }
    public double getWantedX() {
        return wantedX;
    }
    public double getWantedY() {
        return wantedY;
    }
    public double getWantedZ() {
        return wantedZ;
    }
    public void setWait() {
        operation = Operation.WAIT;
    }
    protected static enum Operation {
        WAIT, MOVE_TO, STRAFE, JUMPING;
    }
}

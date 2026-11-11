// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
public class WalkAnimationState {
    private float speedOld;
    private float speed;
    private float position;
    private float positionScale = 1.0F;
    public void setSpeed(float speed) {
        this.speed = speed;
    }
    public void update(float targetSpeed, float factor, float positionScale) {
        speedOld = speed;
        speed += (targetSpeed - speed) * factor;
        position += speed;
        this.positionScale = positionScale;
    }
    public void stop() {
        speedOld = 0.0F;
        speed = 0.0F;
        position = 0.0F;
    }
    public float speed() {
        return speed;
    }
    public float speed(float partialTicks) {
        return Math.min(net.minecraft.util.Mth.lerp(partialTicks, speedOld, speed), 1.0F);
    }
    public float position() {
        return position * positionScale;
    }
    public float position(float partialTicks) {
        return (position - speed * (1.0F - partialTicks)) * positionScale;
    }
    public boolean isMoving() {
        return speed > 1.0E-5F;
    }
}

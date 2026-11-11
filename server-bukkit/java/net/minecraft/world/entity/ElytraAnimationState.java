// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.util.Mth;
public class ElytraAnimationState {
    private float rotX;
    private float rotXOld;
    private float rotY;
    private float rotYOld;
    private float rotZ;
    private float rotZOld;
    public ElytraAnimationState(LivingEntity entity) {
        
    }
    public void tick() {
        
    }
    public float getRotX(float partialTicks) {
        return Mth.lerp(partialTicks, rotXOld, rotX);
    }
    public float getRotY(float partialTicks) {
        return Mth.lerp(partialTicks, rotYOld, rotY);
    }
    public float getRotZ(float partialTicks) {
        return Mth.lerp(partialTicks, rotZOld, rotZ);
    }
}

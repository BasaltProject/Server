// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;

public class Abilities {
    public boolean invulnerable;
    public boolean flying;
    public boolean mayfly;
    public boolean instabuild;
    public boolean mayBuild = true;
    private float flyingSpeed = 0.05F;
    private float walkingSpeed = 0.1F;

    public float getFlyingSpeed() {
        return flyingSpeed;
    }

    public void setFlyingSpeed(float value) {
        flyingSpeed = value;
    }

    public float getWalkingSpeed() {
        return walkingSpeed;
    }

    public void setWalkingSpeed(float value) {
        walkingSpeed = value;
    }

    public Packed pack() {
        return new Packed(invulnerable, flying, mayfly, instabuild, mayBuild, flyingSpeed, walkingSpeed);
    }

    public void apply(Packed packed) {
        invulnerable = packed.invulnerable();
        flying = packed.flying();
        mayfly = packed.mayFly();
        instabuild = packed.instabuild();
        mayBuild = packed.mayBuild();
        flyingSpeed = packed.flyingSpeed();
        walkingSpeed = packed.walkingSpeed();
    }

    public record Packed(boolean invulnerable, boolean flying, boolean mayFly, boolean instabuild, boolean mayBuild,
        float flyingSpeed, float walkingSpeed) {
        public static final Codec<Packed> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            Codec.BOOL.fieldOf("invulnerable").orElse(false).forGetter(Packed::invulnerable),
            Codec.BOOL.fieldOf("flying").orElse(false).forGetter(Packed::flying),
            Codec.BOOL.fieldOf("mayfly").orElse(false).forGetter(Packed::mayFly),
            Codec.BOOL.fieldOf("instabuild").orElse(false).forGetter(Packed::instabuild),
            Codec.BOOL.fieldOf("mayBuild").orElse(true).forGetter(Packed::mayBuild),
            Codec.FLOAT.fieldOf("flySpeed").orElse(0.05F).forGetter(Packed::flyingSpeed),
            Codec.FLOAT.fieldOf("walkSpeed").orElse(0.1F).forGetter(Packed::walkingSpeed)).apply(instance, Packed::new));
    }
}

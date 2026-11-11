// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.function.Consumer;
public enum InsideBlockEffectType {
    FREEZE(entity -> {
        entity.setIsInPowderSnow(true);
        if (entity.canFreeze()) {
            entity.setTicksFrozen(Math.min(entity.getTicksRequiredToFreeze(), entity.getTicksFrozen() + 1));
        }
    }),
    CLEAR_FREEZE(Entity::clearFreeze),
    FIRE_IGNITE(net.minecraft.world.level.block.BaseFireBlock::fireIgnite),
    LAVA_IGNITE(Entity::lavaIgnite),
    EXTINGUISH(Entity::clearFire);
    private final Consumer<Entity> effect;
    InsideBlockEffectType(Consumer<Entity> effect) {
        this.effect = effect;
    }
    public Consumer<Entity> effect() {
        return effect;
    }
}

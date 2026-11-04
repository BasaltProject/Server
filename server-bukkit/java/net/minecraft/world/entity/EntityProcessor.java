// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

@FunctionalInterface
public interface EntityProcessor {
    EntityProcessor NOP = input -> input;

    Entity process(Entity input);
}

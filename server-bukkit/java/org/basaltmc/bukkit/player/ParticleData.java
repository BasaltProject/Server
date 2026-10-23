package org.basaltmc.bukkit.player;

import org.basaltmc.bukkit.BasaltBlockData;
import org.bukkit.Color;
import org.bukkit.Particle;
import org.bukkit.block.data.BlockData;
import org.bukkit.inventory.ItemStack;

final class ParticleData {
    private ParticleData() {}

    static Object[] of(Particle particle, Object data) {
        Class<?> type = particle.getDataType();
        if (type == Void.class) {
            return new Object[0];
        }
        if (data instanceof Particle.DustTransition transition) {
            return new Object[] {(long) transition.getColor().asRGB(), (long) transition.getToColor().asRGB(),
                (double) transition.getSize()};
        }
        if (data instanceof Particle.DustOptions dust) {
            return new Object[] {(long) dust.getColor().asRGB(), (double) dust.getSize()};
        }
        if (data instanceof BlockData block) {
            return new Object[] {(long) BasaltBlockData.stateOf(block)};
        }
        if (data instanceof Color color) {
            return new Object[] {(long) color.asARGB()};
        }
        if (data instanceof ItemStack stack) {
            return new Object[] {stack.getType().getKey().toString()};
        }
        if (data instanceof Float number) {
            return new Object[] {(double) number};
        }
        if (data instanceof Integer number) {
            return new Object[] {(long) number};
        }
        if (data instanceof Particle.Trail trail) {
            return new Object[] {trail.getTarget().getX(), trail.getTarget().getY(), trail.getTarget().getZ(),
                (long) trail.getColor().asRGB(), (long) trail.getDuration()};
        }
        if (data instanceof Particle.Spell spell) {
            return new Object[] {(long) spell.getColor().asARGB(), (double) spell.getPower()};
        }
        if (data == null) {
            throw new IllegalArgumentException("The particle " + particle.getKey() + " needs " + type.getSimpleName());
        }
        throw new IllegalArgumentException("Wrong data " + data + " for the particle " + particle.getKey());
    }
}

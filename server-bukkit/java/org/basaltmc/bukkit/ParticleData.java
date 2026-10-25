package org.basaltmc.bukkit;

import org.bukkit.Color;
import org.bukkit.Location;
import org.bukkit.Particle;
import org.bukkit.Vibration;
import org.bukkit.block.data.BlockData;
import org.bukkit.inventory.ItemStack;

final class ParticleData {
    final double[] numbers;
    final String item;

    private ParticleData(double[] numbers, String item) {
        this.numbers = numbers;
        this.item = item;
    }

    static ParticleData of(Particle particle, Object data) {
        Class<?> expected = particle.getDataType();
        if (expected != Void.class && data == null) {
            throw new IllegalArgumentException("Particle " + particle.getKey() + " requires " + expected.getName());
        }
        if (data == null || expected == Void.class) {
            return new ParticleData(new double[0], "");
        }
        if (!expected.isInstance(data)) {
            throw new IllegalArgumentException("data (" + data.getClass() + ") should be " + expected);
        }
        if (data instanceof BlockData block) {
            return numbers(BasaltBlockData.stateOf(block));
        }
        if (data instanceof Particle.DustTransition transition) {
            return numbers(transition.getColor().asRGB(), transition.getToColor().asRGB(), transition.getSize());
        }
        if (data instanceof Particle.DustOptions dust) {
            return numbers(dust.getColor().asRGB(), dust.getSize());
        }
        if (data instanceof Particle.Spell spell) {
            return numbers(spell.getColor().asARGB(), spell.getPower());
        }
        if (data instanceof Particle.Trail trail) {
            Location target = trail.getTarget();
            return numbers(target.getX(), target.getY(), target.getZ(), trail.getColor().asRGB(), trail.getDuration());
        }
        if (data instanceof Color color) {
            return numbers(color.asARGB());
        }
        if (data instanceof ItemStack stack) {
            return new ParticleData(new double[0], stack.getType().getKey().toString());
        }
        if (data instanceof Number number) {
            return numbers(number.doubleValue());
        }
        if (data instanceof Particle.GeyserBase base) {
            return numbers(base.getWaterBlocks(), base.getBurstImpulse());
        }
        if (data instanceof Particle.Geyser geyser) {
            return numbers(geyser.getWaterBlocks());
        }
        if (data instanceof Vibration vibration) {
            if (vibration.getDestination() instanceof Vibration.Destination.BlockDestination block) {
                Location at = block.getLocation();
                return numbers(at.getBlockX(), at.getBlockY(), at.getBlockZ(), vibration.getArrivalTime());
            }
            throw new IllegalArgumentException("Only a block destination can be shown");
        }
        throw new IllegalArgumentException("Cannot show particle data " + data);
    }

    private static ParticleData numbers(double... values) {
        return new ParticleData(values, "");
    }
}

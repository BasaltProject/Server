// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;

public class NoiseStack implements Noise {
    protected final Layer[] layers;
    private final Interval range;

    private NoiseStack(Layer[] layers) {
        this.layers = layers;
        Interval sum = Interval.ofExact(0.0F);
        for (Layer layer : layers) {
            sum = Interval.add(sum, Interval.mul(layer.noise.range(), Interval.ofExact(layer.amplitude)));
        }
        this.range = sum;
    }

    public static Builder builder() {
        return new Builder();
    }

    public Interval range() {
        return range;
    }

    public float get(double x, double y, double z) {
        float value = 0.0F;
        for (Layer layer : layers) {
            value += layer.amplitude * layer.noise.get(x * layer.frequency, y * layer.frequency, z * layer.frequency);
        }
        return value;
    }

    public float get(double x, double y) {
        float value = 0.0F;
        for (Layer layer : layers) {
            value += layer.amplitude * layer.noise.get(x * layer.frequency, y * layer.frequency);
        }
        return value;
    }

    public void addToVolume(DensityBuffer buffer, DensityVolume volume, double xzScale, double yScale, float amplitude) {
        for (Layer layer : layers) {
            layer.noise.addToVolume(buffer, volume, xzScale * layer.frequency, yScale * layer.frequency,
                amplitude * layer.amplitude);
        }
    }

    public Noise getLayer(int index) {
        return layers[index].noise;
    }

    public static class Builder {
        private final List<Layer> layers = new ArrayList<>();

        private Builder() {
        }

        public Builder add(Noise noise, double frequency, float amplitude) {
            layers.add(new Layer(noise, frequency, amplitude));
            return this;
        }

        public Builder addStack(NoiseStack stack, double frequency, float amplitude) {
            for (Layer layer : stack.layers) {
                layers.add(new Layer(layer.noise, layer.frequency * frequency, layer.amplitude * amplitude));
            }
            return this;
        }

        public NoiseStack build() {
            return new NoiseStack(layers.toArray(new Layer[0]));
        }
    }

    protected record Layer(Noise noise, double frequency, float amplitude) {
    }
}

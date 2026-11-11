// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;
import it.unimi.dsi.fastutil.objects.Reference2ObjectOpenHashMap;
import java.util.Map;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public class EnvironmentAttributeProbe {
    private final Map<EnvironmentAttribute<?>, ValueProbe<?>> probes = new Reference2ObjectOpenHashMap<>();
    private Level level;
    private Vec3 position;
    private final SpatialAttributeInterpolator interpolator = new SpatialAttributeInterpolator();
    public void reset() {
        level = null;
        position = null;
        interpolator.clear();
        probes.clear();
    }
    public void tick(Level level, Vec3 position) {
        this.level = level;
        this.position = position;
        probes.values().removeIf(ValueProbe::tick);
        interpolator.clear();
        var biomes = level.getBiomeManager();
        GaussianSampler.sample(position.scale(0.25), biomes::getNoiseBiomeAtQuart, (weight, biome) -> interpolator.accumulate(weight, biome.value().getAttributes()));
    }
    @SuppressWarnings("unchecked")
    public <Value> Value getValue(EnvironmentAttribute<Value> attribute, float partialTicks) {
        ValueProbe<Value> probe = (ValueProbe<Value>) probes.computeIfAbsent(attribute, a -> new ValueProbe<>(a));
        return probe.get(attribute, partialTicks);
    }
    private class ValueProbe<Value> {
        private Value previous;
        private Value current;
        ValueProbe(EnvironmentAttribute<Value> attribute) {
            Value initial = read(attribute);
            this.previous = initial;
            this.current = initial;
        }
        private Value read(EnvironmentAttribute<Value> attribute) {
            if (level == null || position == null) {
                return attribute.defaultValue();
            }
            return level.environmentAttributes().getValue(attribute, position, interpolator);
        }
        boolean tick() {
            if (current == null) {
                return true;
            }
            previous = current;
            current = null;
            return false;
        }
        Value get(EnvironmentAttribute<Value> attribute, float partialTicks) {
            if (current == null) {
                current = read(attribute);
            }
            return attribute.type().partialTickLerp().apply(partialTicks, previous, current);
        }
    }
}

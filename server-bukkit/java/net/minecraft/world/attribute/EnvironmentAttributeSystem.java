// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;
import java.util.List;
import java.util.Map;
import net.minecraft.core.Holder;
import net.minecraft.world.clock.ClockManager;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.timeline.Timeline;
public class EnvironmentAttributeSystem implements EnvironmentAttributeReader {
    private final Map<EnvironmentAttribute<?>, ValueSampler<?>> samplers = new java.util.HashMap<>();

    private EnvironmentAttributeSystem(Map<EnvironmentAttribute<?>, List<EnvironmentAttributeLayer<?>>> layersByAttribute) {
        layersByAttribute.forEach((attribute, layers) -> samplers.put(attribute, sampler(attribute, layers)));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ValueSampler<?> sampler(EnvironmentAttribute attribute, List layers) {
        Object base = attribute.defaultValue();
        boolean positional = false;
        List<EnvironmentAttributeLayer<Object>> rest = new java.util.ArrayList<>();
        for (Object layer : layers) {
            if (layer instanceof EnvironmentAttributeLayer.Positional) {
                positional = true;
            }
            rest.add((EnvironmentAttributeLayer<Object>) layer);
        }
        return new ValueSampler<>(attribute, base, rest, positional);
    }

    @SuppressWarnings("unchecked")
    private <Value> ValueSampler<Value> samplerFor(EnvironmentAttribute<Value> attribute) {
        return (ValueSampler<Value>) samplers.get(attribute);
    }
    public static Builder builder() {
        return new Builder();
    }
    public void invalidateTickCache() {
        samplers.values().forEach(ValueSampler::invalidateTickCache);
    }
    public <Value> Value getDimensionValue(EnvironmentAttribute<Value> attribute) {
        ValueSampler<Value> sampler = samplerFor(attribute);
        return sampler == null ? attribute.defaultValue() : sampler.getDimensionValue();
    }
    public <Value> Value getValue(EnvironmentAttribute<Value> attribute, Vec3 pos, SpatialAttributeInterpolator biomeInterpolator) {
        ValueSampler<Value> sampler = samplerFor(attribute);
        return sampler == null ? attribute.defaultValue() : sampler.getValue(pos, biomeInterpolator);
    }
    <Value> Value getConstantBaseValue(EnvironmentAttribute<Value> attribute) {
        ValueSampler<Value> sampler = samplerFor(attribute);
        return sampler == null ? attribute.defaultValue() : sampler.baseValue;
    }
    boolean isAffectedByPosition(EnvironmentAttribute<?> attribute) {
        ValueSampler<?> sampler = samplers.get(attribute);
        return sampler != null && sampler.isAffectedByPosition;
    }
    public static class Builder {
        private final Map<EnvironmentAttribute<?>, List<EnvironmentAttributeLayer<?>>> layersByAttribute = new java.util.HashMap<>();

        private Builder() {
        }

        private <Value> Builder addLayer(EnvironmentAttribute<Value> attribute, EnvironmentAttributeLayer<Value> layer) {
            layersByAttribute.computeIfAbsent(attribute, key -> new java.util.ArrayList<>()).add(layer);
            return this;
        }

        private <Value> Builder addEntry(EnvironmentAttribute<Value> attribute, EnvironmentAttributeMap map) {
            EnvironmentAttributeMap.Entry<Value, ?> entry = map.get(attribute);
            return addConstantLayer(attribute, entry::applyModifier);
        }
        public Builder addDefaultLayers(Level level) {
            return this;
        }
        public Builder addStaticLayers(LevelAccessor level) {
            return this;
        }
        public Builder addDynamicLayers(Level level) {
            return this;
        }
        public Builder addConstantLayer(EnvironmentAttributeMap attributeMap) {
            for (EnvironmentAttribute<?> attribute : attributeMap.keySet()) {
                addEntry(attribute, attributeMap);
            }
            return this;
        }
        public <Value> Builder addConstantLayer(EnvironmentAttribute<Value> attribute, EnvironmentAttributeLayer.Constant<Value> layer) {
            return addLayer(attribute, layer);
        }
        public <Value> Builder addTimeBasedLayer(EnvironmentAttribute<Value> attribute, EnvironmentAttributeLayer.TimeBased<Value> layer) {
            return addLayer(attribute, layer);
        }
        public <Value> Builder addPositionalLayer(EnvironmentAttribute<Value> attribute, EnvironmentAttributeLayer.Positional<Value> layer) {
            return addLayer(attribute, layer);
        }
        public Builder addTimelineLayer(Holder<Timeline> timeline, ClockManager clockManager) {
            return this;
        }
        public EnvironmentAttributeSystem build() {
            return new EnvironmentAttributeSystem(layersByAttribute);
        }
    }
    static class ValueSampler<Value> {
        private final EnvironmentAttribute<Value> attribute;
        private final Value baseValue;
        private final List<EnvironmentAttributeLayer<Value>> layers;
        private final boolean isAffectedByPosition;
        private Value cachedTickValue;
        private int cacheTickId;

        private ValueSampler(EnvironmentAttribute<Value> attribute, Value baseValue, List<EnvironmentAttributeLayer<Value>> layers, boolean isAffectedByPosition) {
            this.attribute = attribute;
            this.baseValue = baseValue;
            this.layers = layers;
            this.isAffectedByPosition = isAffectedByPosition;
        }
        public void invalidateTickCache() {
            cachedTickValue = null;
            cacheTickId++;
        }
        public Value getDimensionValue() {
            if (cachedTickValue == null) {
                cachedTickValue = fold(null, null, false);
            }
            return cachedTickValue;
        }
        public Value getValue(Vec3 pos, SpatialAttributeInterpolator biomeInterpolator) {
            return isAffectedByPosition ? fold(pos, biomeInterpolator, true) : getDimensionValue();
        }
        private Value fold(Vec3 pos, SpatialAttributeInterpolator biomeInterpolator, boolean positional) {
            Value value = baseValue;
            for (EnvironmentAttributeLayer<Value> layer : layers) {
                if (layer instanceof EnvironmentAttributeLayer.Constant<Value> constant) {
                    value = constant.applyConstant(value);
                } else if (layer instanceof EnvironmentAttributeLayer.TimeBased<Value> timed) {
                    value = timed.applyTimeBased(value, cacheTickId);
                } else if (positional && layer instanceof EnvironmentAttributeLayer.Positional<Value> spatial) {
                    value = spatial.applyPositional(value, pos, biomeInterpolator);
                }
            }
            return attribute.sanitizeValue(value);
        }
    }
}

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
    private EnvironmentAttributeSystem(Map<EnvironmentAttribute<?>, List<EnvironmentAttributeLayer<?>>> layersByAttribute) {
    }
    public static Builder builder() {
        return new Builder();
    }
    public void invalidateTickCache() {
        return;
    }
    public <Value> Value getDimensionValue(EnvironmentAttribute<Value> attribute) {
        return null;
    }
    public <Value> Value getValue(EnvironmentAttribute<Value> attribute, Vec3 pos, SpatialAttributeInterpolator biomeInterpolator) {
        return null;
    }
    <Value> Value getConstantBaseValue(EnvironmentAttribute<Value> attribute) {
        return null;
    }
    boolean isAffectedByPosition(EnvironmentAttribute<?> attribute) {
        return false;
    }
    public static class Builder {
        private Builder() {
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
            return this;
        }
        public <Value> Builder addConstantLayer(EnvironmentAttribute<Value> attribute, EnvironmentAttributeLayer.Constant<Value> layer) {
            return this;
        }
        public <Value> Builder addTimeBasedLayer(EnvironmentAttribute<Value> attribute, EnvironmentAttributeLayer.TimeBased<Value> layer) {
            return this;
        }
        public <Value> Builder addPositionalLayer(EnvironmentAttribute<Value> attribute, EnvironmentAttributeLayer.Positional<Value> layer) {
            return this;
        }
        public Builder addTimelineLayer(Holder<Timeline> timeline, ClockManager clockManager) {
            return this;
        }
        public EnvironmentAttributeSystem build() {
            return new EnvironmentAttributeSystem(new java.util.HashMap<>());
        }
    }
    static class ValueSampler<Value> {
        private ValueSampler(EnvironmentAttribute<Value> attribute, Value baseValue, List<EnvironmentAttributeLayer<Value>> layers, boolean isAffectedByPosition) {
            
        }
        public void invalidateTickCache() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.attribute.EnvironmentAttributeSystem.ValueSampler#invalidateTickCache");
        }
        public Value getDimensionValue() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.attribute.EnvironmentAttributeSystem.ValueSampler#getDimensionValue");
        }
        public Value getValue(Vec3 pos, SpatialAttributeInterpolator biomeInterpolator) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.attribute.EnvironmentAttributeSystem.ValueSampler#getValue");
        }
    }
}

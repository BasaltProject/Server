// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;
import it.unimi.dsi.fastutil.objects.Reference2DoubleArrayMap;
import it.unimi.dsi.fastutil.objects.Reference2DoubleMap;
import it.unimi.dsi.fastutil.objects.Reference2DoubleMaps;
import java.util.Objects;
public class SpatialAttributeInterpolator {
    private final Reference2DoubleArrayMap<EnvironmentAttributeMap> weights = new Reference2DoubleArrayMap<>();
    public void clear() {
        weights.clear();
    }
    public SpatialAttributeInterpolator accumulate(double weight, EnvironmentAttributeMap attributes) {
        weights.mergeDouble(attributes, weight, Double::sum);
        return this;
    }
    public <Value> Value applyAttributeLayer(EnvironmentAttribute<Value> attribute, Value baseValue) {
        if (weights.isEmpty()) {
            return baseValue;
        }
        if (weights.size() == 1) {
            return weights.keySet().iterator().next().applyModifier(attribute, baseValue);
        }
        LerpFunction<Value> lerp = attribute.type().spatialLerp();
        Value result = null;
        double total = 0.0;
        for (Reference2DoubleMap.Entry<EnvironmentAttributeMap> entry : Reference2DoubleMaps.fastIterable(weights)) {
            Value contribution = entry.getKey().applyModifier(attribute, baseValue);
            total += entry.getDoubleValue();
            result = result == null ? contribution : lerp.apply((float) (entry.getDoubleValue() / total), result, contribution);
        }
        return Objects.requireNonNull(result);
    }
}

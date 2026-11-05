// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import net.minecraft.world.attribute.modifier.AttributeModifier;

public final class EnvironmentAttributeMap {
    public static final EnvironmentAttributeMap EMPTY = new EnvironmentAttributeMap(Map.of());
    public static final Codec<EnvironmentAttributeMap> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(EnvironmentAttributeMap.class, "CODEC");
    public static final Codec<EnvironmentAttributeMap> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(EnvironmentAttributeMap.class, "NETWORK_CODEC");
    public static final Codec<EnvironmentAttributeMap> CODEC_ONLY_POSITIONAL = org.basaltmc.bukkit.facade.LazyCodecs.codec(EnvironmentAttributeMap.class, "CODEC_ONLY_POSITIONAL");

    private final Map<EnvironmentAttribute<?>, Entry<?, ?>> entries;

    private EnvironmentAttributeMap(Map<EnvironmentAttribute<?>, Entry<?, ?>> entries) {
        this.entries = entries;
    }

    public static Builder builder() {
        return new Builder();
    }

    @SuppressWarnings("unchecked")
    public <Value> Entry<Value, ?> get(EnvironmentAttribute<Value> attribute) {
        return (Entry<Value, ?>) entries.get(attribute);
    }

    public <Value> Value applyModifier(EnvironmentAttribute<Value> attribute, Value baseValue) {
        Entry<Value, ?> entry = get(attribute);
        return entry == null ? baseValue : entry.applyModifier(baseValue);
    }

    public boolean contains(EnvironmentAttribute<?> attribute) {
        return entries.containsKey(attribute);
    }

    public Set<EnvironmentAttribute<?>> keySet() {
        return Collections.unmodifiableSet(entries.keySet());
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof EnvironmentAttributeMap other && entries.equals(other.entries);
    }

    @Override
    public int hashCode() {
        return entries.hashCode();
    }

    @Override
    public String toString() {
        return entries.toString();
    }

    public record Entry<Value, Argument>(Argument argument, AttributeModifier<Value, Argument> modifier) {
        public Value applyModifier(Value subject) {
            return modifier.apply(subject, argument);
        }
    }

    public static class Builder {
        private final Map<EnvironmentAttribute<?>, Entry<?, ?>> entries = new LinkedHashMap<>();

        private Builder() {}

        public Builder putAll(EnvironmentAttributeMap map) {
            entries.putAll(map.entries);
            return this;
        }

        public <Value, Parameter> Builder modify(EnvironmentAttribute<Value> attribute, AttributeModifier<Value, Parameter> modifier, Parameter value) {
            attribute.type().checkAllowedModifier(modifier);
            entries.put(attribute, new Entry<>(value, modifier));
            return this;
        }

        public <Value> Builder set(EnvironmentAttribute<Value> attribute, Value value) {
            return modify(attribute, AttributeModifier.override(), value);
        }

        public EnvironmentAttributeMap build() {
            return entries.isEmpty() ? EMPTY : new EnvironmentAttributeMap(new LinkedHashMap<>(entries));
        }
    }
}

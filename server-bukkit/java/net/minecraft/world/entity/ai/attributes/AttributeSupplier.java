// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import com.google.common.collect.ImmutableMap;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;

public class AttributeSupplier {
    private final Map<Holder<Attribute>, AttributeInstance> instances;

    private AttributeSupplier(Map<Holder<Attribute>, AttributeInstance> instances) {
        this.instances = ImmutableMap.copyOf(instances);
    }

    private AttributeInstance getAttributeInstance(Holder<Attribute> attribute) {
        AttributeInstance instance = instances.get(attribute);
        if (instance == null) {
            throw new IllegalArgumentException("Can't find attribute " + Attributes.nameOf(attribute));
        }
        return instance;
    }

    public double getValue(Holder<Attribute> attribute) {
        return getAttributeInstance(attribute).getValue();
    }

    public double getBaseValue(Holder<Attribute> attribute) {
        return getAttributeInstance(attribute).getBaseValue();
    }

    public double getModifierValue(Holder<Attribute> attribute, Identifier id) {
        AttributeModifier modifier = getAttributeInstance(attribute).getModifier(id);
        if (modifier == null) {
            throw new IllegalArgumentException("Can't find modifier " + id + " on attribute " + Attributes.nameOf(attribute));
        }
        return modifier.amount();
    }

    public AttributeInstance createInstance(Consumer<AttributeInstance> onDirty, Holder<Attribute> attribute) {
        AttributeInstance template = instances.get(attribute);
        if (template == null) {
            return null;
        }
        AttributeInstance copy = new AttributeInstance(attribute, onDirty);
        copy.replaceFrom(template);
        return copy;
    }

    public static Builder builder() {
        return new Builder();
    }

    public boolean hasAttribute(Holder<Attribute> attribute) {
        return instances.containsKey(attribute);
    }

    public boolean hasModifier(Holder<Attribute> attribute, Identifier modifier) {
        AttributeInstance instance = instances.get(attribute);
        return instance != null && instance.getModifier(modifier) != null;
    }

    public static class Builder {
        private final Map<Holder<Attribute>, AttributeInstance> builder = new HashMap<>();
        private boolean instanceFrozen;

        private AttributeInstance create(Holder<Attribute> attribute) {
            AttributeInstance instance = new AttributeInstance(attribute, changed -> {
                if (instanceFrozen) {
                    throw new UnsupportedOperationException("Tried to change value for default attribute instance: " + Attributes.nameOf(attribute));
                }
            });
            builder.put(attribute, instance);
            return instance;
        }

        public Builder add(Holder<Attribute> attribute) {
            create(attribute);
            return this;
        }

        public Builder add(Holder<Attribute> attribute, double baseValue) {
            create(attribute).setBaseValue(baseValue);
            return this;
        }

        public AttributeSupplier build() {
            instanceFrozen = true;
            return new AttributeSupplier(builder);
        }
    }
}

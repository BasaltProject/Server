// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import com.google.common.collect.Multimap;
import com.google.common.collect.ImmutableList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;

public class AttributeMap {
    private final Map<Holder<Attribute>, AttributeInstance> attributes = new HashMap<>();
    private final Set<AttributeInstance> attributesToSync = new HashSet<>();
    private final Set<AttributeInstance> attributesToUpdate = new HashSet<>();
    private final AttributeSupplier supplier;

    public AttributeMap(AttributeSupplier supplier) {
        this.supplier = supplier;
    }

    private void onAttributeModified(AttributeInstance instance) {
        attributesToUpdate.add(instance);
        if (instance.getAttribute().value().isClientSyncable()) {
            attributesToSync.add(instance);
        }
    }

    public Set<AttributeInstance> getAttributesToSync() {
        return attributesToSync;
    }

    public Set<AttributeInstance> getAttributesToUpdate() {
        return attributesToUpdate;
    }

    public Collection<AttributeInstance> getSyncableAttributes() {
        return attributes.values().stream().filter(instance -> instance.getAttribute().value().isClientSyncable()).toList();
    }

    public AttributeInstance getInstance(Holder<Attribute> attribute) {
        AttributeInstance found = attributes.get(attribute);
        if (found == null) {
            found = supplier.createInstance(this::onAttributeModified, attribute);
            if (found != null) {
                attributes.put(attribute, found);
            }
        }
        return found;
    }

    public boolean hasAttribute(Holder<Attribute> attribute) {
        return attributes.get(attribute) != null || supplier.hasAttribute(attribute);
    }

    public boolean hasModifier(Holder<Attribute> attribute, Identifier id) {
        AttributeInstance found = attributes.get(attribute);
        return found != null ? found.getModifier(id) != null : supplier.hasModifier(attribute, id);
    }

    public double getValue(Holder<Attribute> attribute) {
        AttributeInstance found = attributes.get(attribute);
        return found != null ? found.getValue() : supplier.getValue(attribute);
    }

    public double getBaseValue(Holder<Attribute> attribute) {
        AttributeInstance found = attributes.get(attribute);
        return found != null ? found.getBaseValue() : supplier.getBaseValue(attribute);
    }

    public double getModifierValue(Holder<Attribute> attribute, Identifier id) {
        AttributeInstance found = attributes.get(attribute);
        if (found == null) {
            return supplier.getModifierValue(attribute, id);
        }
        AttributeModifier modifier = found.getModifier(id);
        if (modifier == null) {
            throw new IllegalArgumentException("Can't find modifier " + id + " on attribute " + Attributes.nameOf(attribute));
        }
        return modifier.amount();
    }

    public void addTransientAttributeModifiers(Multimap<Holder<Attribute>, AttributeModifier> modifiers) {
        modifiers.forEach((attribute, modifier) -> {
            AttributeInstance instance = getInstance(attribute);
            if (instance != null) {
                instance.removeModifier(modifier.id());
                instance.addTransientModifier(modifier);
            }
        });
    }

    public void removeAttributeModifiers(Multimap<Holder<Attribute>, AttributeModifier> modifiers) {
        modifiers.asMap().forEach((attribute, group) -> {
            AttributeInstance instance = attributes.get(attribute);
            if (instance != null) {
                group.forEach(modifier -> instance.removeModifier(modifier.id()));
            }
        });
    }

    public void assignAllValues(AttributeMap other) {
        other.attributes.values().forEach(source -> {
            AttributeInstance target = getInstance(source.getAttribute());
            if (target != null) {
                target.replaceFrom(source);
            }
        });
    }

    public void assignBaseValues(AttributeMap other) {
        other.attributes.values().forEach(source -> {
            AttributeInstance target = getInstance(source.getAttribute());
            if (target != null) {
                target.setBaseValue(source.getBaseValue());
            }
        });
    }

    public void assignPermanentModifiers(AttributeMap other) {
        other.attributes.values().forEach(source -> {
            AttributeInstance target = getInstance(source.getAttribute());
            if (target != null) {
                target.addPermanentModifiers(source.getPermanentModifiers());
            }
        });
    }

    public boolean resetBaseValue(Holder<Attribute> attribute) {
        if (!supplier.hasAttribute(attribute)) {
            return false;
        }
        AttributeInstance instance = attributes.get(attribute);
        if (instance != null) {
            instance.setBaseValue(supplier.getBaseValue(attribute));
        }
        return true;
    }

    public List<AttributeInstance.Packed> pack() {
        ImmutableList.Builder<AttributeInstance.Packed> out = ImmutableList.builder();
        attributes.values().forEach(instance -> out.add(instance.pack()));
        return out.build();
    }

    public void apply(List<AttributeInstance.Packed> packedAttributes) {
        for (AttributeInstance.Packed packed : packedAttributes) {
            AttributeInstance instance = getInstance(packed.attribute());
            if (instance != null) {
                instance.apply(packed);
            }
        }
    }
}

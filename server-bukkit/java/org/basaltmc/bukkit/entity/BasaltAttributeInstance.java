package org.basaltmc.bukkit.entity;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.UUID;
import net.kyori.adventure.key.Key;
import org.bukkit.NamespacedKey;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeInstance;
import org.bukkit.attribute.AttributeModifier;

public final class BasaltAttributeInstance implements AttributeInstance {
    private final EntityCore core;
    private final Attribute attribute;
    private final String id;

    public BasaltAttributeInstance(EntityCore core, Attribute attribute) {
        this.core = core;
        this.attribute = attribute;
        this.id = attribute.getKey().toString();
    }

    private static String operation(AttributeModifier.Operation operation) {
        return switch (operation) {
            case ADD_NUMBER -> "add_value";
            case ADD_SCALAR -> "add_multiplied_base";
            case MULTIPLY_SCALAR_1 -> "add_multiplied_total";
        };
    }

    private static AttributeModifier.Operation operation(String name) {
        return switch (name) {
            case "add_multiplied_base" -> AttributeModifier.Operation.ADD_SCALAR;
            case "add_multiplied_total" -> AttributeModifier.Operation.MULTIPLY_SCALAR_1;
            default -> AttributeModifier.Operation.ADD_NUMBER;
        };
    }

    @Override
    public Attribute getAttribute() {
        return attribute;
    }

    @Override
    public double getBaseValue() {
        return ((Number) core.call("attribute_base", id)).doubleValue();
    }

    @Override
    public void setBaseValue(double value) {
        core.call("set_attribute_base", id, value);
    }

    @Override
    public double getDefaultValue() {
        return ((Number) core.call("attribute_default", id)).doubleValue();
    }

    @Override
    public double getValue() {
        return ((Number) core.call("attribute", id)).doubleValue();
    }

    @Override
    public Collection<AttributeModifier> getModifiers() {
        Object rows = core.call("attribute_modifiers", id);
        List<AttributeModifier> out = new ArrayList<>();
        if (rows instanceof String[] flat) {
            for (int at = 0; at + 2 < flat.length; at += 3) {
                NamespacedKey key = NamespacedKey.fromString(flat[at]);
                out.add(new AttributeModifier(key == null ? new NamespacedKey("minecraft", "modifier") : key,
                    Double.parseDouble(flat[at + 1]), operation(flat[at + 2])));
            }
        }
        return out;
    }

    @Override
    public AttributeModifier getModifier(UUID uuid) {
        return null;
    }

    @Override
    public AttributeModifier getModifier(Key key) {
        for (AttributeModifier modifier : getModifiers()) {
            if (modifier.getKey().toString().equals(key.asString())) {
                return modifier;
            }
        }
        return null;
    }

    @Override
    public void addModifier(AttributeModifier modifier) {
        if (getModifier(Key.key(modifier.getKey().getNamespace(), modifier.getKey().getKey())) != null) {
            throw new IllegalArgumentException("Modifier is already applied on this attribute!");
        }
        core.call("add_attribute_modifier", id, modifier.getKey().toString(), modifier.getAmount(),
            operation(modifier.getOperation()));
    }

    @Override
    public void addTransientModifier(AttributeModifier modifier) {
        addModifier(modifier);
    }

    @Override
    public void removeModifier(AttributeModifier modifier) {
        core.call("remove_attribute_modifier", id, modifier.getKey().toString());
    }

    @Override
    public void removeModifier(UUID uuid) {}

    @Override
    public void removeModifier(Key key) {
        core.call("remove_attribute_modifier", id, key.asString());
    }
}

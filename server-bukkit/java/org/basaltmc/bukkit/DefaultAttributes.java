package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.bukkit.NamespacedKey;
import org.bukkit.attribute.Attributable;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeInstance;
import org.bukkit.attribute.AttributeModifier;

final class DefaultAttributes implements Attributable {
    private final Map<NamespacedKey, Double> base = new HashMap<>();
    private final Map<Attribute, AttributeInstance> made = new HashMap<>();

    DefaultAttributes(String[] defaults) {
        for (String entry : defaults) {
            int equals = entry.lastIndexOf('=');
            base.put(NamespacedKey.fromString(entry.substring(0, equals)),
                Double.parseDouble(entry.substring(equals + 1)));
        }
    }

    @Override
    public synchronized AttributeInstance getAttribute(Attribute attribute) {
        Double value = base.get(attribute.getKey());
        if (value == null) {
            return null;
        }
        return org.basaltmc.bukkit.facade.Memo.get(made, attribute, key -> new Held(key, value));
    }

    @Override
    public synchronized void registerAttribute(Attribute attribute) {
        base.putIfAbsent(attribute.getKey(), attribute.getDefaultValue());
    }

    private static final class Held implements AttributeInstance {
        private final Attribute attribute;
        private final double initial;
        private double value;
        private final List<AttributeModifier> modifiers = new ArrayList<>();

        Held(Attribute attribute, double initial) {
            this.attribute = attribute;
            this.initial = initial;
            this.value = initial;
        }

        @Override
        public Attribute getAttribute() {
            return attribute;
        }

        @Override
        public double getBaseValue() {
            return value;
        }

        @Override
        public void setBaseValue(double base) {
            value = base;
        }

        @Override
        public Collection<AttributeModifier> getModifiers() {
            return List.copyOf(modifiers);
        }

        @Override
        public void addModifier(AttributeModifier modifier) {
            modifiers.add(modifier);
        }

        @Override
        public void removeModifier(AttributeModifier modifier) {
            modifiers.remove(modifier);
        }

        @Override
        public double getValue() {
            double add = value;
            double scale = 1.0;
            double total = 1.0;
            for (AttributeModifier modifier : modifiers) {
                switch (modifier.getOperation()) {
                    case ADD_NUMBER -> add += modifier.getAmount();
                    case ADD_SCALAR -> scale += modifier.getAmount();
                    default -> total *= 1.0 + modifier.getAmount();
                }
            }
            double[] range = KeyedFacts.attributeRange(attribute.getKey().getKey());
            double result = add * scale * total;
            return range == null ? result : Math.max(range[1], Math.min(range[2], result));
        }

        @Override
        public double getDefaultValue() {
            return initial;
        }

        @Override
        public AttributeModifier getModifier(net.kyori.adventure.key.Key key) {
            for (AttributeModifier modifier : modifiers) {
                if (modifier.getKey().equals(key)) {
                    return modifier;
                }
            }
            return null;
        }

        @Override
        public void removeModifier(net.kyori.adventure.key.Key key) {
            modifiers.removeIf(modifier -> modifier.getKey().equals(key));
        }

        @Override
        public AttributeModifier getModifier(java.util.UUID id) {
            return null;
        }

        @Override
        public void removeModifier(java.util.UUID id) {}

        @Override
        public void addTransientModifier(AttributeModifier modifier) {
            modifiers.add(modifier);
        }
    }
}

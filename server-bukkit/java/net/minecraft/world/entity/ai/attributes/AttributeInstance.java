// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Collection;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;

public class AttributeInstance {
    private final Holder<Attribute> attribute;
    private final Map<AttributeModifier.Operation, Map<Identifier, AttributeModifier>> modifiersByOperation =
        new EnumMap<>(AttributeModifier.Operation.class);
    private final Map<Identifier, AttributeModifier> modifierById = new HashMap<>();
    private final Map<Identifier, AttributeModifier> permanentModifiers = new HashMap<>();
    private double baseValue;
    private boolean dirty = true;
    private double cachedValue;
    private final Consumer<AttributeInstance> onDirty;

    public AttributeInstance(Holder<Attribute> attribute, Consumer<AttributeInstance> onDirty) {
        this.attribute = attribute;
        this.onDirty = onDirty;
        this.baseValue = attribute.value().getDefaultValue();
    }

    public Holder<Attribute> getAttribute() {
        return attribute;
    }

    public double getBaseValue() {
        return baseValue;
    }

    public void setBaseValue(double baseValue) {
        if (baseValue != this.baseValue) {
            this.baseValue = baseValue;
            setDirty();
        }
    }

    Map<Identifier, AttributeModifier> getModifiers(AttributeModifier.Operation operation) {
        return modifiersByOperation.computeIfAbsent(operation, key -> new HashMap<>());
    }

    public Set<AttributeModifier> getModifiers() {
        return new LinkedHashSet<>(modifierById.values());
    }

    public Set<AttributeModifier> getPermanentModifiers() {
        return new LinkedHashSet<>(permanentModifiers.values());
    }

    public AttributeModifier getModifier(Identifier id) {
        return modifierById.get(id);
    }

    public boolean hasModifier(Identifier modifier) {
        return modifierById.containsKey(modifier);
    }

    private void addModifier(AttributeModifier modifier) {
        AttributeModifier existing = modifierById.putIfAbsent(modifier.id(), modifier);
        if (existing != null) {
            throw new IllegalArgumentException("Modifier is already applied on this attribute!");
        }
        getModifiers(modifier.operation()).put(modifier.id(), modifier);
        setDirty();
    }

    public void addOrUpdateTransientModifier(AttributeModifier modifier) {
        AttributeModifier previous = modifierById.put(modifier.id(), modifier);
        if (modifier == previous) {
            return;
        }
        if (previous != null) {
            getModifiers(previous.operation()).remove(modifier.id());
        }
        getModifiers(modifier.operation()).put(modifier.id(), modifier);
        setDirty();
    }

    public void addTransientModifier(AttributeModifier modifier) {
        addModifier(modifier);
    }

    public void addOrReplacePermanentModifier(AttributeModifier modifier) {
        removeModifier(modifier.id());
        addModifier(modifier);
        permanentModifiers.put(modifier.id(), modifier);
    }

    public void addPermanentModifier(AttributeModifier modifier) {
        addModifier(modifier);
        permanentModifiers.put(modifier.id(), modifier);
    }

    public void addPermanentModifiers(Collection<AttributeModifier> modifiers) {
        for (AttributeModifier modifier : modifiers) {
            addPermanentModifier(modifier);
        }
    }

    protected void setDirty() {
        dirty = true;
        onDirty.accept(this);
    }

    public void removeModifier(AttributeModifier modifier) {
        removeModifier(modifier.id());
    }

    public boolean removeModifier(Identifier id) {
        AttributeModifier removed = modifierById.remove(id);
        if (removed == null) {
            return false;
        }
        getModifiers(removed.operation()).remove(id);
        permanentModifiers.remove(id);
        setDirty();
        return true;
    }

    public void removeModifiers() {
        for (AttributeModifier modifier : getModifiers()) {
            removeModifier(modifier);
        }
    }

    public double getValue() {
        if (dirty) {
            cachedValue = calculateValue();
            dirty = false;
        }
        return cachedValue;
    }

    private double calculateValue() {
        double base = getBaseValue();
        for (AttributeModifier modifier : getModifiersOrEmpty(AttributeModifier.Operation.ADD_VALUE)) {
            base += modifier.amount();
        }
        double value = base;
        for (AttributeModifier modifier : getModifiersOrEmpty(AttributeModifier.Operation.ADD_MULTIPLIED_BASE)) {
            value += base * modifier.amount();
        }
        for (AttributeModifier modifier : getModifiersOrEmpty(AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL)) {
            value *= 1.0 + modifier.amount();
        }
        return attribute.value().sanitizeValue(value);
    }

    private Collection<AttributeModifier> getModifiersOrEmpty(AttributeModifier.Operation operation) {
        Map<Identifier, AttributeModifier> found = modifiersByOperation.get(operation);
        return found == null ? List.of() : found.values();
    }

    public void replaceFrom(AttributeInstance other) {
        baseValue = other.baseValue;
        modifierById.clear();
        modifierById.putAll(other.modifierById);
        permanentModifiers.clear();
        permanentModifiers.putAll(other.permanentModifiers);
        modifiersByOperation.clear();
        other.modifiersByOperation.forEach((operation, map) -> getModifiers(operation).putAll(map));
        setDirty();
    }

    public Packed pack() {
        return new Packed(attribute, baseValue, List.copyOf(permanentModifiers.values()));
    }

    public void apply(Packed packed) {
        baseValue = packed.baseValue();
        for (AttributeModifier modifier : packed.modifiers()) {
            modifierById.put(modifier.id(), modifier);
            getModifiers(modifier.operation()).put(modifier.id(), modifier);
            permanentModifiers.put(modifier.id(), modifier);
        }
        setDirty();
    }

    public static record Packed(Holder<Attribute> attribute, double baseValue, List<AttributeModifier> modifiers) {
        public static final Codec<Packed> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            Attribute.CODEC.fieldOf("id").forGetter(Packed::attribute),
            Codec.DOUBLE.fieldOf("base").orElse(0.0).forGetter(Packed::baseValue),
            AttributeModifier.CODEC.listOf().optionalFieldOf("modifiers", List.of()).forGetter(Packed::modifiers))
            .apply(instance, Packed::new));
        public static final Codec<List<Packed>> LIST_CODEC = CODEC.listOf();
    }
}

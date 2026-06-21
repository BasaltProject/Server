package org.basaltmc.bukkit.item;

import com.google.common.collect.LinkedHashMultimap;
import com.google.common.collect.Multimap;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.function.Predicate;
import org.bukkit.NamespacedKey;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeModifier;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.EquipmentSlotGroup;

/** The `minecraft:attribute_modifiers` list of an item, read and written as Bukkit's attribute modifiers. */
public final class MetaAttributes {
    private static final String kAttributes = "minecraft:attribute_modifiers";

    private MetaAttributes() {}

    static boolean has(Map<String, Object> c) {
        List<Object> list = Nbt.list(c.get(kAttributes));
        return list != null && !list.isEmpty();
    }

    public static Attribute attribute(String id) {
        NamespacedKey key = NamespacedKey.fromString(id);
        return key == null ? null : org.bukkit.Registry.ATTRIBUTE.get(key);
    }

    public static AttributeModifier modifier(Map<String, Object> entry) {
        NamespacedKey id = NamespacedKey.fromString(Nbt.string(entry, "id", "minecraft:unnamed"));
        AttributeModifier.Operation operation;
        switch (Nbt.string(entry, "operation", "add_value")) {
            case "add_multiplied_base":
                operation = AttributeModifier.Operation.ADD_SCALAR;
                break;
            case "add_multiplied_total":
                operation = AttributeModifier.Operation.MULTIPLY_SCALAR_1;
                break;
            default:
                operation = AttributeModifier.Operation.ADD_NUMBER;
                break;
        }
        EquipmentSlotGroup slot = EquipmentSlotGroup.getByName(Nbt.string(entry, "slot", "any"));
        return new AttributeModifier(id, Nbt.real(entry, "amount", 0), operation,
            slot == null ? EquipmentSlotGroup.ANY : slot);
    }

    public static Map<String, Object> entry(Attribute attribute, AttributeModifier modifier) {
        Map<String, Object> entry = Nbt.compound();
        entry.put("type", attribute.getKey().toString());
        entry.put("id", modifier.getKey().toString());
        entry.put("amount", modifier.getAmount());
        switch (modifier.getOperation()) {
            case ADD_SCALAR:
                entry.put("operation", "add_multiplied_base");
                break;
            case MULTIPLY_SCALAR_1:
                entry.put("operation", "add_multiplied_total");
                break;
            default:
                entry.put("operation", "add_value");
                break;
        }
        entry.put("slot", modifier.getSlotGroup().toString());
        return entry;
    }

    public static Multimap<Attribute, AttributeModifier> all(Map<String, Object> c, Predicate<Map<String, Object>> keep) {
        Multimap<Attribute, AttributeModifier> out = LinkedHashMultimap.create();
        List<Object> list = Nbt.list(c.get(kAttributes));
        if (list != null) {
            for (Object item : list) {
                Map<String, Object> entry = Nbt.map(item);
                if (entry != null && keep.test(entry)) {
                    Attribute attribute = attribute(Nbt.string(entry, "type", ""));
                    if (attribute != null) {
                        out.put(attribute, modifier(entry));
                    }
                }
            }
        }
        return out;
    }

    public static Multimap<Attribute, AttributeModifier> forSlot(Map<String, Object> c, EquipmentSlot slot) {
        if (slot == null) {
            throw new IllegalArgumentException("EquipmentSlot cannot be null");
        }
        return all(c, entry -> {
            EquipmentSlotGroup group = EquipmentSlotGroup.getByName(Nbt.string(entry, "slot", "any"));
            return group == null || group.test(slot);
        });
    }

    static Collection<AttributeModifier> forAttribute(Map<String, Object> c, Attribute attribute) {
        if (attribute == null) {
            throw new IllegalArgumentException("Attribute cannot be null");
        }
        String id = attribute.getKey().toString();
        Multimap<Attribute, AttributeModifier> found = all(c, entry -> id.equals(Nbt.string(entry, "type", "")));
        return found.isEmpty() ? null : found.values();
    }

    static boolean add(Map<String, Object> c, Attribute attribute, AttributeModifier modifier) {
        if (attribute == null || modifier == null) {
            throw new IllegalArgumentException("Attribute and modifier cannot be null");
        }
        List<Object> list = Nbt.list(c.get(kAttributes));
        if (list == null) {
            list = new ArrayList<>();
            c.put(kAttributes, list);
        }
        String id = modifier.getKey().toString();
        for (Object item : list) {
            Map<String, Object> existing = Nbt.map(item);
            if (existing != null && id.equals(Nbt.string(existing, "id", ""))
                && attribute.getKey().toString().equals(Nbt.string(existing, "type", ""))) {
                return false;
            }
        }
        list.add(entry(attribute, modifier));
        return true;
    }

    static void set(Map<String, Object> c, Multimap<Attribute, AttributeModifier> modifiers) {
        if (modifiers == null || modifiers.isEmpty()) {
            c.remove(kAttributes);
            return;
        }
        List<Object> list = new ArrayList<>();
        for (Map.Entry<Attribute, AttributeModifier> pair : modifiers.entries()) {
            if (pair.getKey() != null && pair.getValue() != null) {
                list.add(entry(pair.getKey(), pair.getValue()));
            }
        }
        c.put(kAttributes, list);
    }

    private static boolean removeWhere(Map<String, Object> c, Predicate<Map<String, Object>> drop) {
        List<Object> list = Nbt.list(c.get(kAttributes));
        if (list == null) {
            return false;
        }
        boolean changed = list.removeIf(item -> item instanceof Map && drop.test(Nbt.map(item)));
        if (list.isEmpty()) {
            c.remove(kAttributes);
        }
        return changed;
    }

    static boolean removeAttribute(Map<String, Object> c, Attribute attribute) {
        String id = attribute.getKey().toString();
        return removeWhere(c, entry -> id.equals(Nbt.string(entry, "type", "")));
    }

    static boolean removeSlot(Map<String, Object> c, EquipmentSlot slot) {
        return removeWhere(c, entry -> {
            EquipmentSlotGroup group = EquipmentSlotGroup.getByName(Nbt.string(entry, "slot", "any"));
            return group == null || group.test(slot);
        });
    }

    static boolean remove(Map<String, Object> c, Attribute attribute, AttributeModifier modifier) {
        String type = attribute.getKey().toString();
        String id = modifier.getKey().toString();
        return removeWhere(c, entry -> type.equals(Nbt.string(entry, "type", ""))
            && id.equals(Nbt.string(entry, "id", "")));
    }
}

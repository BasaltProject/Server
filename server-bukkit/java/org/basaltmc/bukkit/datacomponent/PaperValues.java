package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.datacomponent.DataComponentType;
import io.papermc.paper.datacomponent.item.CustomModelData;
import io.papermc.paper.datacomponent.item.DamageResistant;
import io.papermc.paper.datacomponent.item.DyedItemColor;
import io.papermc.paper.datacomponent.item.Enchantable;
import io.papermc.paper.datacomponent.item.FoodProperties;
import io.papermc.paper.datacomponent.item.ItemEnchantments;
import io.papermc.paper.datacomponent.item.ItemLore;
import io.papermc.paper.datacomponent.item.MapId;
import io.papermc.paper.datacomponent.item.OminousBottleAmplifier;
import io.papermc.paper.datacomponent.item.Repairable;
import io.papermc.paper.datacomponent.item.TooltipDisplay;
import io.papermc.paper.datacomponent.item.UseCooldown;
import io.papermc.paper.datacomponent.item.UseRemainder;
import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.set.RegistryKeySet;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.ComponentLike;
import net.kyori.adventure.text.format.NamedTextColor;
import net.kyori.adventure.text.format.TextDecoration;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.KeySet;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Color;
import org.bukkit.NamespacedKey;
import org.bukkit.damage.DamageType;
import org.bukkit.enchantments.Enchantment;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.ItemType;

/** The Paper component values the server can hold, each knowing the stored form it was read from and goes back to. */
final class PaperValues {
    private PaperValues() {}

    interface Stored {
        Object nbt();
    }

    static final class Lore implements ItemLore, Stored {
        final List<Component> lines;

        Lore(List<Component> lines) {
            this.lines = List.copyOf(lines);
        }

        @Override
        public List<Component> lines() {
            return lines;
        }

        @Override
        public List<Component> styledLines() {
            List<Component> out = new ArrayList<>();
            for (Component line : lines) {
                out.add(line.colorIfAbsent(NamedTextColor.DARK_PURPLE).decorationIfAbsent(TextDecoration.ITALIC,
                    TextDecoration.State.TRUE));
            }
            return out;
        }

        @Override
        public Object nbt() {
            List<Object> out = new ArrayList<>();
            boolean allMaps = true;
            for (Component line : lines) {
                Object nbt = Texts.toNbt(line);
                out.add(nbt);
                allMaps &= nbt instanceof Map;
            }
            if (!allMaps) {
                for (int at = 0; at < out.size(); at++) {
                    if (!(out.get(at) instanceof Map)) {
                        Map<String, Object> wrapped = Nbt.compound();
                        wrapped.put("", out.get(at));
                        out.set(at, wrapped);
                    }
                }
            }
            return out;
        }

        static Lore of(Object nbt) {
            List<Component> lines = new ArrayList<>();
            List<Object> list = Nbt.list(nbt);
            if (list != null) {
                for (Object line : list) {
                    lines.add(Texts.fromNbt(line));
                }
            }
            return new Lore(lines);
        }
    }

    static final class LoreBuilder implements ItemLore.Builder {
        private final List<Component> lines = new ArrayList<>();

        @Override
        public ItemLore.Builder lines(List<? extends ComponentLike> lines) {
            this.lines.clear();
            return addLines(lines);
        }

        @Override
        public ItemLore.Builder addLine(ComponentLike line) {
            lines.add(line.asComponent());
            return this;
        }

        @Override
        public ItemLore.Builder addLines(List<? extends ComponentLike> lines) {
            for (ComponentLike line : lines) {
                this.lines.add(line.asComponent());
            }
            return this;
        }

        @Override
        public ItemLore build() {
            return new Lore(lines);
        }
    }

    static final class Enchants implements ItemEnchantments, Stored {
        final Map<Enchantment, Integer> levels;

        Enchants(Map<Enchantment, Integer> levels) {
            this.levels = java.util.Collections.unmodifiableMap(new LinkedHashMap<>(levels));
        }

        @Override
        public Map<Enchantment, Integer> enchantments() {
            return levels;
        }

        @Override
        public Object nbt() {
            Map<String, Object> out = Nbt.compound();
            for (Map.Entry<Enchantment, Integer> entry : levels.entrySet()) {
                out.put(entry.getKey().getKey().toString(), entry.getValue());
            }
            return out;
        }

        static Enchants of(Object nbt) {
            Map<Enchantment, Integer> levels = new LinkedHashMap<>();
            Map<String, Object> map = Nbt.map(nbt);
            if (map != null) {
                for (Map.Entry<String, Object> entry : map.entrySet()) {
                    NamespacedKey key = NamespacedKey.fromString(entry.getKey());
                    Enchantment enchantment = key == null ? null : Enchantment.getByKey(key);
                    if (enchantment != null && entry.getValue() instanceof Number level) {
                        levels.put(enchantment, level.intValue());
                    }
                }
            }
            return new Enchants(levels);
        }
    }

    static final class EnchantsBuilder implements ItemEnchantments.Builder {
        private final Map<Enchantment, Integer> levels = new LinkedHashMap<>();

        @Override
        public ItemEnchantments.Builder add(Enchantment enchantment, int level) {
            levels.put(enchantment, level);
            return this;
        }

        @Override
        public ItemEnchantments.Builder addAll(Map<Enchantment, Integer> enchantments) {
            levels.putAll(enchantments);
            return this;
        }

        @Override
        public ItemEnchantments build() {
            return new Enchants(levels);
        }
    }

    static final class Model implements CustomModelData, Stored {
        final Map<String, Object> data;

        Model(Map<String, Object> data) {
            this.data = data;
        }

        private List<Object> list(String key) {
            List<Object> found = Nbt.list(data.get(key));
            return found == null ? List.of() : found;
        }

        @Override
        public List<Float> floats() {
            List<Float> out = new ArrayList<>();
            for (Object value : list("floats")) {
                out.add(((Number) value).floatValue());
            }
            return List.copyOf(out);
        }

        @Override
        public List<Boolean> flags() {
            List<Boolean> out = new ArrayList<>();
            for (Object value : list("flags")) {
                out.add(((Number) value).intValue() != 0);
            }
            return List.copyOf(out);
        }

        @Override
        public List<String> strings() {
            List<String> out = new ArrayList<>();
            for (Object value : list("strings")) {
                out.add(String.valueOf(value));
            }
            return List.copyOf(out);
        }

        @Override
        public List<Color> colors() {
            List<Color> out = new ArrayList<>();
            for (Object value : list("colors")) {
                out.add(Color.fromRGB(((Number) value).intValue() & 0xFFFFFF));
            }
            return List.copyOf(out);
        }

        @Override
        public Object nbt() {
            return Nbt.deepCopy(data);
        }
    }

    static final class ModelBuilder implements CustomModelData.Builder {
        private final List<Object> floats = new ArrayList<>();
        private final List<Object> flags = new ArrayList<>();
        private final List<Object> strings = new ArrayList<>();
        private final List<Object> colors = new ArrayList<>();

        @Override
        public CustomModelData.Builder addFloat(float f) {
            floats.add(f);
            return this;
        }

        @Override
        public CustomModelData.Builder addFloats(List<Float> values) {
            floats.addAll(values);
            return this;
        }

        @Override
        public CustomModelData.Builder addFlag(boolean flag) {
            flags.add(Nbt.bool(flag));
            return this;
        }

        @Override
        public CustomModelData.Builder addFlags(List<Boolean> values) {
            for (Boolean value : values) {
                addFlag(value);
            }
            return this;
        }

        @Override
        public CustomModelData.Builder addString(String string) {
            strings.add(string);
            return this;
        }

        @Override
        public CustomModelData.Builder addStrings(List<String> values) {
            strings.addAll(values);
            return this;
        }

        @Override
        public CustomModelData.Builder addColor(Color color) {
            colors.add(color.asRGB());
            return this;
        }

        @Override
        public CustomModelData.Builder addColors(List<Color> values) {
            for (Color value : values) {
                addColor(value);
            }
            return this;
        }

        @Override
        public CustomModelData build() {
            Map<String, Object> data = Nbt.compound();
            if (!floats.isEmpty()) {
                data.put("floats", new ArrayList<>(floats));
            }
            if (!flags.isEmpty()) {
                data.put("flags", new ArrayList<>(flags));
            }
            if (!strings.isEmpty()) {
                data.put("strings", new ArrayList<>(strings));
            }
            if (!colors.isEmpty()) {
                data.put("colors", new ArrayList<>(colors));
            }
            return new Model(data);
        }
    }

    static final class Dyed implements DyedItemColor, Stored {
        final Color color;

        Dyed(Color color) {
            this.color = color;
        }

        @Override
        public Color color() {
            return color;
        }

        @Override
        public Object nbt() {
            return color.asRGB();
        }
    }

    static final class DyedBuilder implements DyedItemColor.Builder {
        private Color color = Color.WHITE;

        @Override
        public DyedItemColor.Builder color(Color color) {
            this.color = color;
            return this;
        }

        @Override
        public DyedItemColor build() {
            return new Dyed(color);
        }
    }

    static final class Tooltip implements TooltipDisplay, Stored {
        final boolean hide;
        final Set<DataComponentType> hidden;

        Tooltip(boolean hide, Set<DataComponentType> hidden) {
            this.hide = hide;
            this.hidden = Set.copyOf(hidden);
        }

        @Override
        public boolean hideTooltip() {
            return hide;
        }

        @Override
        public Set<DataComponentType> hiddenComponents() {
            return hidden;
        }

        @Override
        public Object nbt() {
            Map<String, Object> out = Nbt.compound();
            if (hide) {
                out.put("hide_tooltip", Nbt.bool(true));
            }
            if (!hidden.isEmpty()) {
                List<Object> names = new ArrayList<>();
                for (DataComponentType type : hidden) {
                    names.add(type.getKey().toString());
                }
                out.put("hidden_components", names);
            }
            return out;
        }

        static Tooltip of(Object nbt) {
            Map<String, Object> map = Nbt.map(nbt);
            Set<DataComponentType> hidden = new LinkedHashSet<>();
            List<Object> names = map == null ? null : Nbt.list(map.get("hidden_components"));
            if (names != null) {
                for (Object name : names) {
                    DataComponentType type = ComponentType.of(String.valueOf(name));
                    if (type != null) {
                        hidden.add(type);
                    }
                }
            }
            return new Tooltip(Nbt.flag(map, "hide_tooltip", false), hidden);
        }
    }

    static final class TooltipBuilder implements TooltipDisplay.Builder {
        private boolean hide;
        private final Set<DataComponentType> hidden = new LinkedHashSet<>();

        @Override
        public TooltipDisplay.Builder hideTooltip(boolean hide) {
            this.hide = hide;
            return this;
        }

        @Override
        public TooltipDisplay.Builder addHiddenComponents(DataComponentType... components) {
            hidden.addAll(List.of(components));
            return this;
        }

        @Override
        public TooltipDisplay.Builder hiddenComponents(Set<DataComponentType> components) {
            hidden.clear();
            hidden.addAll(components);
            return this;
        }

        @Override
        public TooltipDisplay build() {
            return new Tooltip(hide, hidden);
        }
    }

    record EnchantableValue(int value) implements Enchantable, Stored {
        @Override
        public Object nbt() {
            Map<String, Object> out = Nbt.compound();
            out.put("value", value);
            return out;
        }
    }

    record MapIdValue(int id) implements MapId, Stored {
        @Override
        public Object nbt() {
            return id;
        }
    }

    record AmplifierValue(int amplifier) implements OminousBottleAmplifier, Stored {
        @Override
        public Object nbt() {
            return amplifier;
        }
    }

    static final class Cooldown implements UseCooldown, Stored {
        final float seconds;
        final Key group;

        Cooldown(float seconds, Key group) {
            this.seconds = seconds;
            this.group = group;
        }

        @Override
        public float seconds() {
            return seconds;
        }

        @Override
        public Key cooldownGroup() {
            return group;
        }

        @Override
        public Object nbt() {
            Map<String, Object> out = Nbt.compound();
            out.put("seconds", seconds);
            if (group != null) {
                out.put("cooldown_group", group.asString());
            }
            return out;
        }
    }

    static final class CooldownBuilder implements UseCooldown.Builder {
        private final float seconds;
        private Key group;

        CooldownBuilder(float seconds) {
            this.seconds = seconds;
        }

        @Override
        public UseCooldown.Builder cooldownGroup(Key key) {
            this.group = key;
            return this;
        }

        @Override
        public UseCooldown build() {
            return new Cooldown(seconds, group);
        }
    }

    static final class Food implements FoodProperties, Stored {
        final int nutrition;
        final float saturation;
        final boolean always;

        Food(int nutrition, float saturation, boolean always) {
            this.nutrition = nutrition;
            this.saturation = saturation;
            this.always = always;
        }

        @Override
        public int nutrition() {
            return nutrition;
        }

        @Override
        public float saturation() {
            return saturation;
        }

        @Override
        public boolean canAlwaysEat() {
            return always;
        }

        @Override
        public FoodProperties.Builder toBuilder() {
            return new FoodBuilder().nutrition(nutrition).saturation(saturation).canAlwaysEat(always);
        }

        @Override
        public Object nbt() {
            Map<String, Object> out = Nbt.compound();
            out.put("nutrition", nutrition);
            out.put("saturation", saturation);
            out.put("can_always_eat", Nbt.bool(always));
            return out;
        }
    }

    static final class FoodBuilder implements FoodProperties.Builder {
        private int nutrition;
        private float saturation;
        private boolean always;

        @Override
        public FoodProperties.Builder canAlwaysEat(boolean canAlwaysEat) {
            this.always = canAlwaysEat;
            return this;
        }

        @Override
        public FoodProperties.Builder saturation(float saturation) {
            this.saturation = saturation;
            return this;
        }

        @Override
        public FoodProperties.Builder nutrition(int nutrition) {
            this.nutrition = nutrition;
            return this;
        }

        @Override
        public FoodProperties build() {
            return new Food(nutrition, saturation, always);
        }
    }

    record RepairableValue(RegistryKeySet<ItemType> types) implements Repairable, Stored {
        @Override
        public Object nbt() {
            List<Object> ids = new ArrayList<>();
            for (var key : types.values()) {
                ids.add(key.key().asString());
            }
            return ids;
        }

        static RepairableValue of(Object nbt) {
            List<String> ids = new ArrayList<>();
            if (nbt instanceof List<?> list) {
                for (Object id : list) {
                    ids.add(String.valueOf(id));
                }
            } else if (nbt instanceof String single) {
                ids.add(single);
            }
            return new RepairableValue(new KeySet<>(RegistryKey.ITEM, ids));
        }
    }

    record DamageResistantValue(RegistryKeySet<DamageType> types) implements DamageResistant, Stored {
        @Override
        public Object nbt() {
            Map<String, Object> out = Nbt.compound();
            List<Object> ids = new ArrayList<>();
            for (var key : types.values()) {
                ids.add(key.key().asString());
            }
            out.put("types", ids);
            return out;
        }

        static DamageResistantValue of(Object nbt) {
            Map<String, Object> map = Nbt.map(nbt);
            List<String> ids = new ArrayList<>();
            Object types = map == null ? null : map.get("types");
            if (types instanceof List<?> list) {
                for (Object id : list) {
                    ids.add(String.valueOf(id));
                }
            } else if (types instanceof String single) {
                ids.add(single);
            }
            return new DamageResistantValue(new KeySet<>(RegistryKey.DAMAGE_TYPE, ids));
        }
    }

    record RemainderValue(ItemStack transformInto) implements UseRemainder, Stored {
        @Override
        public Object nbt() {
            return Convert.stack(transformInto);
        }

        static RemainderValue of(Object nbt) {
            Map<String, Object> map = Nbt.map(nbt);
            ItemStack stack = map == null ? null : Convert.stack(map);
            return new RemainderValue(stack == null ? ItemStack.empty() : stack);
        }
    }
}

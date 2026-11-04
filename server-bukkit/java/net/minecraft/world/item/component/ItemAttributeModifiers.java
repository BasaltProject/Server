// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.entity.player.Player;
import org.apache.commons.lang3.function.TriConsumer;
import org.basaltmc.bukkit.facade.HolderRefs;

public record ItemAttributeModifiers(List<Entry> modifiers) {
    public static final ItemAttributeModifiers EMPTY = new ItemAttributeModifiers(List.of());
    public static final Codec<ItemAttributeModifiers> CODEC = Entry.CODEC.listOf().xmap(ItemAttributeModifiers::new,
        ItemAttributeModifiers::modifiers);
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemAttributeModifiers> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.ItemAttributeModifiers.class, "STREAM_CODEC");
    public static final DecimalFormat ATTRIBUTE_MODIFIER_FORMAT = new DecimalFormat("#.##", DecimalFormatSymbols.getInstance(Locale.ROOT));

    public static Builder builder() {
        return new Builder();
    }

    public ItemAttributeModifiers withModifierAdded(Holder<Attribute> attribute, AttributeModifier modifier, EquipmentSlotGroup slot) {
        List<Entry> next = new ArrayList<>(modifiers);
        next.add(new Entry(attribute, modifier, slot));
        return new ItemAttributeModifiers(List.copyOf(next));
    }

    public void forEach(EquipmentSlotGroup slot, TriConsumer<Holder<Attribute>, AttributeModifier, Display> consumer) {
        for (Entry entry : modifiers) {
            if (entry.slot == slot) {
                consumer.accept(entry.attribute, entry.modifier, entry.display);
            }
        }
    }

    public void forEach(EquipmentSlotGroup slot, BiConsumer<Holder<Attribute>, AttributeModifier> consumer) {
        for (Entry entry : modifiers) {
            if (entry.slot == slot) {
                consumer.accept(entry.attribute, entry.modifier);
            }
        }
    }

    public void forEach(EquipmentSlot slot, BiConsumer<Holder<Attribute>, AttributeModifier> consumer) {
        for (Entry entry : modifiers) {
            if (entry.slot.test(slot)) {
                consumer.accept(entry.attribute, entry.modifier);
            }
        }
    }

    public double compute(Holder<Attribute> attribute, double baseValue, EquipmentSlot slot) {
        double added = baseValue;
        double multipliedBase = 0.0;
        double multipliedTotal = 1.0;
        for (Entry entry : modifiers) {
            if (!entry.attribute.is(attribute) || !entry.slot.test(slot)) {
                continue;
            }
            switch (entry.modifier.operation()) {
                case ADD_VALUE -> added += entry.modifier.amount();
                case ADD_MULTIPLIED_BASE -> multipliedBase += entry.modifier.amount();
                case ADD_MULTIPLIED_TOTAL -> multipliedTotal *= 1.0 + entry.modifier.amount();
            }
        }
        return (added + added * multipliedBase) * multipliedTotal;
    }

    public interface Display {
        Codec<Display> CODEC = new Codec<>() {
            @Override
            public <T> DataResult<com.mojang.datafixers.util.Pair<Display, T>> decode(DynamicOps<T> ops, T input) {
                DataResult<MapLike<T>> map = ops.getMap(input);
                if (map.error().isPresent()) {
                    return DataResult.error(() -> "Display is not a map");
                }
                T type = map.result().get().get("type");
                String name = type == null ? "default" : ops.getStringValue(type).result().orElse("default");
                switch (name) {
                    case "default":
                        return DataResult.success(com.mojang.datafixers.util.Pair.of(attributeModifiers(), input));
                    case "hidden":
                        return DataResult.success(com.mojang.datafixers.util.Pair.of(hidden(), input));
                    case "override":
                        T value = map.result().get().get("value");
                        return value == null ? DataResult.error(() -> "Missing override value")
                            : ComponentSerialization.CODEC.parse(ops, value).map(component ->
                                com.mojang.datafixers.util.Pair.of(override(component), input));
                    default:
                        return DataResult.error(() -> "Unknown display type " + name);
                }
            }

            @Override
            public <T> DataResult<T> encode(Display input, DynamicOps<T> ops, T prefix) {
                java.util.Map<T, T> out = new java.util.LinkedHashMap<>();
                out.put(ops.createString("type"), ops.createString(input.type().getSerializedName()));
                if (input instanceof OverrideText text) {
                    ComponentSerialization.CODEC.encodeStart(ops, text.component()).result().ifPresent(
                        value -> out.put(ops.createString("value"), value));
                }
                return DataResult.success(ops.createMap(out));
            }
        };
        StreamCodec<RegistryFriendlyByteBuf, Display> STREAM_CODEC = StreamCodec.of(
            (buf, display) -> {
                buf.writeVarInt(display.type().ordinal());
                if (display instanceof OverrideText text) {
                    ComponentSerialization.STREAM_CODEC.encode(buf, text.component());
                }
            },
            buf -> switch (buf.readVarInt()) {
                case 1 -> hidden();
                case 2 -> override(ComponentSerialization.STREAM_CODEC.decode(buf));
                default -> attributeModifiers();
            });

        static Display attributeModifiers() {
            return Default.INSTANCE;
        }

        static Display hidden() {
            return Hidden.INSTANCE;
        }

        static Display override(Component component) {
            return new OverrideText(component);
        }

        Type type();

        void apply(Consumer<Component> consumer, Player player, Holder<Attribute> attribute, AttributeModifier modifier);

        public static enum Type implements StringRepresentable {
            DEFAULT("default"), HIDDEN("hidden"), OVERRIDE("override");

            private final String name;

            Type(String name) {
                this.name = name;
            }

            @Override
            public String getSerializedName() {
                return name;
            }
        }

        public static record Default() implements Display {
            static final Default INSTANCE = new Default();

            @Override
            public Type type() {
                return Type.DEFAULT;
            }

            @Override
            public void apply(Consumer<Component> consumer, Player player, Holder<Attribute> attribute, AttributeModifier modifier) {
            }
        }

        public static record Hidden() implements Display {
            static final Hidden INSTANCE = new Hidden();

            @Override
            public Type type() {
                return Type.HIDDEN;
            }

            @Override
            public void apply(Consumer<Component> consumer, Player player, Holder<Attribute> attribute, AttributeModifier modifier) {
            }
        }

        public static record OverrideText(Component component) implements Display {
            @Override
            public Type type() {
                return Type.OVERRIDE;
            }

            @Override
            public void apply(Consumer<Component> consumer, Player player, Holder<Attribute> attribute, AttributeModifier modifier) {
                consumer.accept(component);
            }
        }
    }

    public static record Entry(Holder<Attribute> attribute, AttributeModifier modifier, EquipmentSlotGroup slot, Display display) {
        public static final Codec<Entry> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            Identifier.CODEC.<Holder<Attribute>>xmap(id -> HolderRefs.of(Attribute.class, id),
                holder -> holder.unwrapKey().get().identifier()).fieldOf("type").forGetter(Entry::attribute),
            Identifier.CODEC.fieldOf("id").forGetter(entry -> entry.modifier.id()),
            Codec.DOUBLE.fieldOf("amount").forGetter(entry -> entry.modifier.amount()),
            AttributeModifier.Operation.CODEC.fieldOf("operation").forGetter(entry -> entry.modifier.operation()),
            EquipmentSlotGroup.CODEC.optionalFieldOf("slot", EquipmentSlotGroup.ANY).forGetter(Entry::slot),
            Display.CODEC.optionalFieldOf("display", Display.attributeModifiers()).forGetter(Entry::display))
            .apply(instance, (attribute, id, amount, operation, slot, display) ->
                new Entry(attribute, new AttributeModifier(id, amount, operation), slot, display)));
        public static final StreamCodec<RegistryFriendlyByteBuf, Entry> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.ItemAttributeModifiers.Entry.class, "STREAM_CODEC");

        public Entry(Holder<Attribute> attribute, AttributeModifier modifier, EquipmentSlotGroup slot) {
            this(attribute, modifier, slot, Display.attributeModifiers());
        }

        public boolean matches(Holder<Attribute> attribute, Identifier id) {
            return attribute.is(this.attribute) && modifier.is(id);
        }
    }

    public static class Builder {
        private final List<Entry> entries = new ArrayList<>();

        private Builder() {}

        public Builder add(Holder<Attribute> attribute, AttributeModifier modifier, EquipmentSlotGroup slot) {
            entries.add(new Entry(attribute, modifier, slot));
            return this;
        }

        public Builder add(Holder<Attribute> attribute, AttributeModifier modifier, EquipmentSlotGroup slot, Display display) {
            entries.add(new Entry(attribute, modifier, slot, display));
            return this;
        }

        public ItemAttributeModifiers build() {
            return new ItemAttributeModifiers(List.copyOf(entries));
        }
    }
}

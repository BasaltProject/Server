// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting.display;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.context.ContextMap;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.equipment.trim.TrimPattern;
public interface SlotDisplay {
    Codec<SlotDisplay> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.display.SlotDisplay.class, "CODEC");
    StreamCodec<RegistryFriendlyByteBuf, SlotDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.class, "STREAM_CODEC");
    <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> builder);
    Type<? extends SlotDisplay> type();
    default boolean isEnabled(FeatureFlagSet enabledFeatures) {
        return true;
    }
    default List<ItemStack> resolveForStacks(ContextMap context) {
        return resolve(context, ItemStackContentsFactory.INSTANCE).toList();
    }
    default ItemStack resolveForFirstStack(ContextMap context) {
        return resolve(context, ItemStackContentsFactory.INSTANCE).findFirst().orElse(ItemStack.EMPTY);
    }
    public static record Type<T extends SlotDisplay>(MapCodec<T> codec, StreamCodec<RegistryFriendlyByteBuf, T> streamCodec) {
    }
    public static class ItemStackContentsFactory implements DisplayContentsFactory.ForStacks<ItemStack> {
        public static final ItemStackContentsFactory INSTANCE = new ItemStackContentsFactory();
        public ItemStack forStack(ItemStack stack) {
            return stack;
        }
    }
    public static class Empty implements SlotDisplay {
        public static final Empty INSTANCE = new Empty();
        public static final MapCodec<Empty> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.Empty.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Empty> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.Empty.class, "STREAM_CODEC");
        public static final Type<Empty> TYPE = new Type<>(Empty.MAP_CODEC, Empty.STREAM_CODEC);
        private Empty() {
            
        }
        public Type<Empty> type() {
            return TYPE;
        }
        public String toString() {
            return super.toString();
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return Stream.empty();
        }
    }
    public static class AnyFuel implements SlotDisplay {
        public static final AnyFuel INSTANCE = new AnyFuel();
        public static final MapCodec<AnyFuel> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.AnyFuel.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, AnyFuel> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.AnyFuel.class, "STREAM_CODEC");
        public static final Type<AnyFuel> TYPE = new Type<>(AnyFuel.MAP_CODEC, AnyFuel.STREAM_CODEC);
        private AnyFuel() {
            
        }
        public Type<AnyFuel> type() {
            return TYPE;
        }
        public String toString() {
            return super.toString();
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return Stream.empty();
        }
    }
    public static record WithAnyPotion(SlotDisplay display) implements SlotDisplay {
        public static final MapCodec<WithAnyPotion> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.WithAnyPotion.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, WithAnyPotion> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.WithAnyPotion.class, "STREAM_CODEC");
        public static final Type<WithAnyPotion> TYPE = new Type<>(WithAnyPotion.MAP_CODEC, WithAnyPotion.STREAM_CODEC);
        public Type<WithAnyPotion> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            if (!(factory instanceof DisplayContentsFactory.ForStacks<T> stacks)) {
            return Stream.empty();
        }
        List<ItemStack> bases = display.resolve(context, ItemStackContentsFactory.INSTANCE).toList();
        return net.minecraft.core.registries.BuiltInRegistries.POTION.listElements().flatMap(potion -> bases.stream().map(base -> {
            ItemStack stack = base.copy();
            stack.set(net.minecraft.core.component.DataComponents.POTION_CONTENTS, new net.minecraft.world.item.alchemy.PotionContents(potion));
            return stacks.forStack(stack);
        }));
        }
    }
    public static record OnlyWithComponent(SlotDisplay source, DataComponentType<?> component) implements SlotDisplay {
        public static final MapCodec<OnlyWithComponent> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.OnlyWithComponent.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, OnlyWithComponent> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.OnlyWithComponent.class, "STREAM_CODEC");
        public static final Type<OnlyWithComponent> TYPE = new Type<>(OnlyWithComponent.MAP_CODEC, OnlyWithComponent.STREAM_CODEC);
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            if (!(factory instanceof DisplayContentsFactory.ForStacks<T> stacks)) {
            return Stream.empty();
        }
        return source.resolve(context, ItemStackContentsFactory.INSTANCE).filter(stack -> stack.has(component)).map(stacks::forStack);
        }
        public Type<OnlyWithComponent> type() {
            return TYPE;
        }
    }
    public static record DyedSlotDemo(SlotDisplay dye, SlotDisplay target) implements SlotDisplay {
        public static final MapCodec<DyedSlotDemo> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.DyedSlotDemo.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, DyedSlotDemo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.DyedSlotDemo.class, "STREAM_CODEC");
        public static final Type<DyedSlotDemo> TYPE = new Type<>(DyedSlotDemo.MAP_CODEC, DyedSlotDemo.STREAM_CODEC);
        public Type<DyedSlotDemo> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return Stream.empty();
        }
    }
    public static record SmithingTrimDemoSlotDisplay(SlotDisplay base, SlotDisplay material, Holder<TrimPattern> pattern) implements SlotDisplay {
        public static final MapCodec<SmithingTrimDemoSlotDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.SmithingTrimDemoSlotDisplay.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, SmithingTrimDemoSlotDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.SmithingTrimDemoSlotDisplay.class, "STREAM_CODEC");
        public static final Type<SmithingTrimDemoSlotDisplay> TYPE = new Type<>(SmithingTrimDemoSlotDisplay.MAP_CODEC, SmithingTrimDemoSlotDisplay.STREAM_CODEC);
        public Type<SmithingTrimDemoSlotDisplay> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return Stream.empty();
        }
    }
    public static record ItemSlotDisplay(Holder<Item> item) implements SlotDisplay {
        public static final MapCodec<ItemSlotDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.ItemSlotDisplay.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, ItemSlotDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.ItemSlotDisplay.class, "STREAM_CODEC");
        public static final Type<ItemSlotDisplay> TYPE = new Type<>(ItemSlotDisplay.MAP_CODEC, ItemSlotDisplay.STREAM_CODEC);
        public ItemSlotDisplay(Item item) {
            this(item.builtInRegistryHolder());
        }
        public Type<ItemSlotDisplay> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return factory instanceof DisplayContentsFactory.ForStacks<T> stacks ? Stream.of(stacks.forStack(item)) : Stream.empty();
        }
        public boolean isEnabled(FeatureFlagSet enabledFeatures) {
            return item.value().isEnabled(enabledFeatures);
        }
    }
    public static record ItemStackSlotDisplay(ItemStackTemplate stack) implements SlotDisplay {
        public static final MapCodec<ItemStackSlotDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.ItemStackSlotDisplay.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, ItemStackSlotDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.ItemStackSlotDisplay.class, "STREAM_CODEC");
        public static final Type<ItemStackSlotDisplay> TYPE = new Type<>(ItemStackSlotDisplay.MAP_CODEC, ItemStackSlotDisplay.STREAM_CODEC);
        public Type<ItemStackSlotDisplay> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return factory instanceof DisplayContentsFactory.ForStacks<T> stacks ? Stream.of(stacks.forStack(stack.create())) : Stream.empty();
        }
        public boolean isEnabled(FeatureFlagSet enabledFeatures) {
            return stack.item().value().isEnabled(enabledFeatures);
        }
    }
    public static record TagSlotDisplay(HolderSet<Item> tag) implements SlotDisplay {
        public static final MapCodec<TagSlotDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.TagSlotDisplay.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, TagSlotDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.TagSlotDisplay.class, "STREAM_CODEC");
        public static final Type<TagSlotDisplay> TYPE = new Type<>(TagSlotDisplay.MAP_CODEC, TagSlotDisplay.STREAM_CODEC);
        public Type<TagSlotDisplay> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return factory instanceof DisplayContentsFactory.ForStacks<T> stacks ? tag.stream().map(stacks::forStack) : Stream.empty();
        }
    }
    public static record Composite(List<SlotDisplay> contents) implements SlotDisplay {
        public static final MapCodec<Composite> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.Composite.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Composite> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.Composite.class, "STREAM_CODEC");
        public static final Type<Composite> TYPE = new Type<>(Composite.MAP_CODEC, Composite.STREAM_CODEC);
        public Type<Composite> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            return contents.stream().flatMap(entry -> entry.resolve(context, factory));
        }
        public boolean isEnabled(FeatureFlagSet enabledFeatures) {
            return contents.stream().allMatch(entry -> entry.isEnabled(enabledFeatures));
        }
    }
    public static record WithRemainder(SlotDisplay input, SlotDisplay remainder) implements SlotDisplay {
        public static final MapCodec<WithRemainder> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.SlotDisplay.WithRemainder.class, "MAP_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, WithRemainder> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.SlotDisplay.WithRemainder.class, "STREAM_CODEC");
        public static final Type<WithRemainder> TYPE = new Type<>(WithRemainder.MAP_CODEC, WithRemainder.STREAM_CODEC);
        public Type<WithRemainder> type() {
            return TYPE;
        }
        public <T> Stream<T> resolve(ContextMap context, DisplayContentsFactory<T> factory) {
            if (!(factory instanceof DisplayContentsFactory.ForRemainders<T> remainders)) {
            return input.resolve(context, factory);
        }
        List<T> extra = remainder.resolve(context, factory).toList();
        return input.resolve(context, factory).map(entry -> remainders.addRemainder(entry, extra));
        }
        public boolean isEnabled(FeatureFlagSet enabledFeatures) {
            return input.isEnabled(enabledFeatures) && remainder.isEnabled(enabledFeatures);
        }
    }
}

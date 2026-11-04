// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import java.util.function.Function;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.state.BlockBehaviour;
import org.apache.commons.lang3.function.TriFunction;

public record ColorCollection<T>(T white, T orange, T magenta, T lightBlue, T yellow, T lime, T pink, T gray,
        T lightGray, T cyan, T purple, T blue, T brown, T green, T red, T black) {
    public static final ColorCollection<DyeColor> VALUES = from(color -> color);
    public static final ColorCollection<String> NAMES = from(DyeColor::getName);

    public static <T> ColorCollection<T> from(Function<DyeColor, T> factory) {
        List<T> values = java.util.Arrays.stream(DyeColor.values()).map(factory).toList();
        return new ColorCollection<>(values.get(0), values.get(1), values.get(2), values.get(3), values.get(4),
            values.get(5), values.get(6), values.get(7), values.get(8), values.get(9), values.get(10), values.get(11),
            values.get(12), values.get(13), values.get(14), values.get(15));
    }

    public static <T> ColorCollection<T> create(T value) {
        return from(color -> value);
    }

    public static <B extends Block, Id> ColorCollection<Block> registerBlocks(ColorCollection<Id> ids,
            TriFunction<Id, Function<BlockBehaviour.Properties, Block>, BlockBehaviour.Properties, Block> register,
            BiFunction<DyeColor, BlockBehaviour.Properties, B> colorBlockFactory,
            Function<DyeColor, BlockBehaviour.Properties> propertiesSupplier) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.ColorCollection#registerBlocks");
    }

    public static <Id> ColorCollection<Item> registerBlockItems(ColorCollection<Id> ids, ColorCollection<Block> blocks,
            TriFunction<Id, Block, DyeColor, Item> itemFactory) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.ColorCollection#registerBlockItems");
    }

    public static <Id> ColorCollection<Item> registerItems(ColorCollection<Id> ids,
            BiFunction<Id, DyeColor, Item> itemFactory) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.ColorCollection#registerItems");
    }

    public static ColorCollection<String> prefixWithColor(ColorCollection<String> ids) {
        return zipMap(NAMES, ids, (color, id) -> color + "_" + id);
    }

    public List<T> asList() {
        return List.of(white, orange, magenta, lightBlue, yellow, lime, pink, gray, lightGray, cyan, purple, blue,
            brown, green, red, black);
    }

    public void forEach(Consumer<T> consumer) {
        asList().forEach(consumer);
    }

    public T pick(DyeColor dyeColor) {
        return asList().get(dyeColor.getId());
    }

    public <U> ColorCollection<U> map(Function<T, U> mapper) {
        return from(color -> mapper.apply(pick(color)));
    }

    public static <T, U> void zipApply(ColorCollection<T> first, ColorCollection<U> second, BiConsumer<T, U> consumer) {
        for (DyeColor color : DyeColor.values()) {
            consumer.accept(first.pick(color), second.pick(color));
        }
    }

    public static <T, U, R> ColorCollection<R> zipMap(ColorCollection<T> first, ColorCollection<U> second,
            BiFunction<T, U, R> operation) {
        return from(color -> operation.apply(first.pick(color), second.pick(color)));
    }
}

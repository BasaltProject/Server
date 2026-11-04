// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.Function;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
public interface ResolvableInt {
    Codec<ResolvableInt> CODEC = com.mojang.serialization.Codec.either(com.mojang.serialization.Codec.INT, net.minecraft.resources.Identifier.CODEC).xmap(
        either -> either.map(value -> (ResolvableInt) new Constant(value), id -> (ResolvableInt) new Reference(
            net.minecraft.resources.ResourceKey.create(net.minecraft.resources.ResourceKey.createRegistryKey(
                net.minecraft.resources.Identifier.withDefaultNamespace("context_int_provider")), id))),
        resolvable -> resolvable instanceof Constant constant ? com.mojang.datafixers.util.Either.left(constant.value())
            : com.mojang.datafixers.util.Either.right(((Reference) resolvable).key().identifier()));
    StreamCodec<ByteBuf, ResolvableInt> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.storage.loot.providers.number.ints.ResolvableInt.class, "STREAM_CODEC");
    int get(LootContext context, int defaultValue);
    static ResolvableInt fromKey(ResourceKey<ContextIntProvider> key) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ResolvableInt#fromKey");
    }
    static <T> int getFromItem(ItemStack itemStack, DataComponentType<T> componentType, Function<T, ResolvableInt> getter, LootContext context, int defaultValue) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ResolvableInt#getFromItem");
    }
    public static record Constant(int value) implements ResolvableInt {
        public int get(LootContext context, int defaultValue) {
            return value;
        }
    }
    public static record Reference(ResourceKey<ContextIntProvider> key) implements ResolvableInt {
        public int get(LootContext context, int defaultValue) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ResolvableInt.Reference#get");
        }
    }
}

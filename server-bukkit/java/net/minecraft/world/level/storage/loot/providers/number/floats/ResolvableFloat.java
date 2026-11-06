// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.Function;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
public interface ResolvableFloat {
    Codec<ResolvableFloat> CODEC = com.mojang.serialization.Codec.either(com.mojang.serialization.Codec.FLOAT, net.minecraft.resources.Identifier.CODEC).xmap(
        either -> either.map(value -> (ResolvableFloat) new Constant(value), id -> (ResolvableFloat) new Reference(
            net.minecraft.resources.ResourceKey.create(net.minecraft.resources.ResourceKey.createRegistryKey(
                net.minecraft.resources.Identifier.withDefaultNamespace("context_float_provider")), id))),
        resolvable -> resolvable instanceof Constant constant ? com.mojang.datafixers.util.Either.left(constant.value())
            : com.mojang.datafixers.util.Either.right(((Reference) resolvable).key().identifier()));
    StreamCodec<ByteBuf, ResolvableFloat> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.storage.loot.providers.number.floats.ResolvableFloat.class, "STREAM_CODEC");
    float get(LootContext context, float defaultValue);
    static ResolvableFloat fromKey(ResourceKey<ContextFloatProvider> key) {
        return new Reference(key);
    }
    static <T> float getFromItem(ItemStack itemStack, DataComponentType<T> componentType, Function<T, ResolvableFloat> getter, LootContext context, float defaultValue) {
        T value = itemStack.get(componentType);
        return value == null ? defaultValue : getter.apply(value).get(context, defaultValue);
    }
    public static record Constant(float value) implements ResolvableFloat {
        public float get(LootContext context, float defaultValue) {
            return value;
        }
    }
    public static record Reference(ResourceKey<ContextFloatProvider> key) implements ResolvableFloat {
        public float get(LootContext context, float defaultValue) {
            return context.getResolver().lookup(net.minecraft.core.registries.Registries.CONTEXT_FLOAT_PROVIDER)
                .flatMap(lookup -> lookup.get(key)).map(holder -> holder.value().getFloat(context)).orElse(defaultValue);
        }
    }
}

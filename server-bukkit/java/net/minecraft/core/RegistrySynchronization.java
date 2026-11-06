// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;
import net.minecraft.network.codec.ByteBufCodecs;
import com.mojang.serialization.DynamicOps;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.stream.Stream;
import net.minecraft.nbt.Tag;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.RegistryLayer;
import net.minecraft.server.packs.repository.KnownPack;
public class RegistrySynchronization {
    private static final Set<ResourceKey<? extends Registry<?>>> kNetworkable = new java.util.HashSet<>();
    static {
        net.minecraft.resources.RegistryDataLoader.SYNCHRONIZED_REGISTRIES.forEach(data -> kNetworkable.add(data.key()));
    }
    public static void packRegistries(DynamicOps<Tag> ops, RegistryAccess registries, Set<KnownPack> clientKnownPacks, BiConsumer<ResourceKey<? extends Registry<?>>, List<PackedRegistryEntry>> output) {
        net.minecraft.resources.RegistryDataLoader.SYNCHRONIZED_REGISTRIES.forEach(data -> packRegistry(ops, data, registries, clientKnownPacks, output));
    }
    private static <T> void packRegistry(DynamicOps<Tag> ops, net.minecraft.resources.RegistryDataLoader.RegistryData<T> data, RegistryAccess registries, Set<KnownPack> clientKnownPacks, BiConsumer<ResourceKey<? extends Registry<?>>, List<PackedRegistryEntry>> output) {
        registries.lookup(data.key()).ifPresent(registry -> {
            List<PackedRegistryEntry> packed = new java.util.ArrayList<>(registry.size());
            registry.listElements().forEach(element -> {
                boolean known = registry.registrationInfo(element.key()).flatMap(RegistrationInfo::knownPackInfo).filter(clientKnownPacks::contains).isPresent();
                Optional<Tag> contents = known ? Optional.empty()
                    : Optional.of(data.elementCodec().encodeStart(ops, element.value()).getOrThrow(message -> new IllegalArgumentException("Failed to serialize " + element.key() + ": " + message)));
                packed.add(new PackedRegistryEntry(element.key().identifier(), contents));
            });
            output.accept(registry.key(), packed);
        });
    }
    private static Stream<RegistryAccess.RegistryEntry<?>> ownedNetworkableRegistries(RegistryAccess access) {
        return access.registries().filter(entry -> isNetworkable(entry.key()));
    }
    public static Stream<RegistryAccess.RegistryEntry<?>> networkedRegistries(LayeredRegistryAccess<RegistryLayer> registries) {
        return ownedNetworkableRegistries(registries.getAccessFrom(RegistryLayer.WORLD));
    }
    public static Stream<RegistryAccess.RegistryEntry<?>> networkSafeRegistries(LayeredRegistryAccess<RegistryLayer> registries) {
        return Stream.concat(networkedRegistries(registries), registries.getLayer(RegistryLayer.STATIC).registries());
    }
    public static boolean isNetworkable(ResourceKey<? extends Registry<?>> key) {
        return kNetworkable.contains(key);
    }
    public static record PackedRegistryEntry(Identifier id, Optional<Tag> data) {
        public static final StreamCodec<ByteBuf, PackedRegistryEntry> STREAM_CODEC = StreamCodec.composite(Identifier.STREAM_CODEC, PackedRegistryEntry::id, ByteBufCodecs.optional(ByteBufCodecs.TRUSTED_TAG),
            PackedRegistryEntry::data, PackedRegistryEntry::new);
    }
}

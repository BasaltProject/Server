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
    public static void packRegistries(DynamicOps<Tag> ops, RegistryAccess registries, Set<KnownPack> clientKnownPacks, BiConsumer<ResourceKey<? extends Registry<?>>, List<PackedRegistryEntry>> output) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.RegistrySynchronization#packRegistries");
    }
    public static Stream<RegistryAccess.RegistryEntry<?>> networkedRegistries(LayeredRegistryAccess<RegistryLayer> registries) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.RegistrySynchronization#networkedRegistries");
    }
    public static Stream<RegistryAccess.RegistryEntry<?>> networkSafeRegistries(LayeredRegistryAccess<RegistryLayer> registries) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.RegistrySynchronization#networkSafeRegistries");
    }
    public static boolean isNetworkable(ResourceKey<? extends Registry<?>> key) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.RegistrySynchronization#isNetworkable");
    }
    public static record PackedRegistryEntry(Identifier id, Optional<Tag> data) {
        public static final StreamCodec<ByteBuf, PackedRegistryEntry> STREAM_CODEC = StreamCodec.composite(Identifier.STREAM_CODEC, PackedRegistryEntry::id, ByteBufCodecs.optional(ByteBufCodecs.TRUSTED_TAG),
            PackedRegistryEntry::data, PackedRegistryEntry::new);
    }
}

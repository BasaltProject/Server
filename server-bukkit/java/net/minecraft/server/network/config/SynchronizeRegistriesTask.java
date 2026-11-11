// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network.config;

import java.util.List;
import java.util.Set;
import java.util.function.Consumer;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.RegistrySynchronization;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.common.ClientboundUpdateTagsPacket;
import net.minecraft.network.protocol.configuration.ClientboundRegistryDataPacket;
import net.minecraft.network.protocol.configuration.ClientboundSelectKnownPacks;
import net.minecraft.server.RegistryLayer;
import net.minecraft.server.network.ConfigurationTask;
import net.minecraft.server.packs.repository.KnownPack;
import net.minecraft.tags.TagNetworkSerialization;

public class SynchronizeRegistriesTask implements ConfigurationTask {
    public static final ConfigurationTask.Type TYPE = new ConfigurationTask.Type("synchronize_registries");
    private final List<KnownPack> offered;
    private final LayeredRegistryAccess<RegistryLayer> registries;

    public SynchronizeRegistriesTask(List<KnownPack> knownPacks, LayeredRegistryAccess<RegistryLayer> registries) {
        this.offered = knownPacks;
        this.registries = registries;
    }

    public void start(Consumer<Packet<?>> connection) {
        connection.accept(new ClientboundSelectKnownPacks(offered));
    }

    private void sendRegistries(Consumer<Packet<?>> connection, Set<KnownPack> shared) {
        var ops = registries.compositeAccess().createSerializationContext(NbtOps.INSTANCE);
        RegistrySynchronization.packRegistries(ops, registries.getAccessFrom(RegistryLayer.WORLD), shared,
            (key, entries) -> connection.accept(new ClientboundRegistryDataPacket(key, entries)));
        connection.accept(new ClientboundUpdateTagsPacket(TagNetworkSerialization.serializeTagsToNetwork(registries)));
    }

    public void handleResponse(List<KnownPack> acceptedPacks, Consumer<Packet<?>> connection) {
        sendRegistries(connection, acceptedPacks.equals(offered) ? Set.copyOf(offered) : Set.of());
    }

    public ConfigurationTask.Type type() {
        return TYPE;
    }
}

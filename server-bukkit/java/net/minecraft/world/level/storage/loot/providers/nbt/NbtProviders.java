// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.nbt;
import com.mojang.datafixers.util.Either;
import net.minecraft.core.registries.BuiltInRegistries;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Registry;
public class NbtProviders {
    public static final Codec<NbtProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.nbt.NbtProviders.class, "CODEC");
    public static MapCodec<? extends NbtProvider> bootstrap(Registry<MapCodec<? extends NbtProvider>> registry) {
        Registry.register(registry, "storage", StorageNbtProvider.MAP_CODEC);
        return Registry.register(registry, "context", ContextNbtProvider.MAP_CODEC);
    }
}

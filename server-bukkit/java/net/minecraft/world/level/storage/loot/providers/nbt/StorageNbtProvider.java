// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.nbt;
import com.mojang.serialization.MapCodec;
import net.minecraft.nbt.Tag;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.storage.loot.LootContext;
public record StorageNbtProvider(Identifier id) implements NbtProvider {
    public static final MapCodec<StorageNbtProvider> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.nbt.StorageNbtProvider.class, "MAP_CODEC");
    public MapCodec<StorageNbtProvider> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Tag get(LootContext context) {
        return context.getLevel().getServer().getCommandStorage().get(id);
    }
}

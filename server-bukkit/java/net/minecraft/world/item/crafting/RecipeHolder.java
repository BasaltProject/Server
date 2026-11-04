// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
public record RecipeHolder<T extends Recipe<?>>(ResourceKey<Recipe<?>> id, T value) {
    public static final StreamCodec<RegistryFriendlyByteBuf, RecipeHolder<?>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.RecipeHolder.class, "STREAM_CODEC");
    public boolean equals(Object obj) {
        return obj instanceof RecipeHolder<?> other && id.equals(other.id);
    }
    public int hashCode() {
        return id.hashCode();
    }
    public String toString() {
        return id.toString();
    }
}

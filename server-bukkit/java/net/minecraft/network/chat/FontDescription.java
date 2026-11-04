// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.component.ResolvableProfile;
public interface FontDescription {
    Codec<FontDescription> CODEC = Identifier.CODEC.flatComapMap(Resource::new, description ->
        description instanceof Resource resource ? DataResult.success(resource.id())
            : DataResult.error(() -> "Unsupported font description type: " + description));
    Resource DEFAULT = new Resource(Identifier.withDefaultNamespace("default"));
    public static record Resource(Identifier id) implements FontDescription {
    }
    public static record AtlasSprite(Identifier atlasId, Identifier spriteId) implements FontDescription {
    }
    public static record PlayerSprite(ResolvableProfile profile, boolean hat) implements FontDescription {
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;

public interface ClientAsset {
    Identifier id();

    public static record ResourceTexture(Identifier id, Identifier texturePath) implements Texture {
        public static final Codec<ResourceTexture> CODEC = Identifier.CODEC.xmap(ResourceTexture::new, ResourceTexture::id);
        public static final MapCodec<ResourceTexture> DEFAULT_FIELD_CODEC = CODEC.fieldOf("texture");
        public static final StreamCodec<ByteBuf, ResourceTexture> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.STRING_UTF8
            .map(text -> new ResourceTexture(Identifier.parse(text)), texture -> texture.id().toString());

        public ResourceTexture(Identifier texture) {
            this(texture, texture.withPath(path -> "textures/" + path + ".png"));
        }
    }

    public static record DownloadedTexture(Identifier texturePath, String url) implements Texture {
        @Override
        public Identifier id() {
            return texturePath;
        }
    }

    public interface Texture extends ClientAsset {
        Identifier texturePath();
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.core.ClientAsset;
import net.minecraft.network.codec.StreamCodec;
public record PlayerSkin(ClientAsset.Texture body, ClientAsset. Texture cape, ClientAsset. Texture elytra, PlayerModelType model, boolean secure) {
    public static PlayerSkin insecure(ClientAsset.Texture body, ClientAsset.Texture cape, ClientAsset.Texture elytra, PlayerModelType model) {
        return new PlayerSkin(body, cape, elytra, model, false);
    }

    public PlayerSkin with(Patch patch) {
        return patch == Patch.EMPTY ? this : new PlayerSkin(patch.body().map(texture -> (ClientAsset.Texture) texture).orElse(body),
            patch.cape().map(texture -> (ClientAsset.Texture) texture).orElse(cape),
            patch.elytra().map(texture -> (ClientAsset.Texture) texture).orElse(elytra), patch.model().orElse(model), secure);
    }

    public static record Patch(Optional<ClientAsset.ResourceTexture> body, Optional<ClientAsset.ResourceTexture> cape, Optional<ClientAsset.ResourceTexture> elytra, Optional<PlayerModelType> model) {
        public static final Patch EMPTY = new Patch(Optional.empty(), Optional.empty(), Optional.empty(), Optional.empty());
        public static final MapCodec<Patch> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
            ClientAsset.ResourceTexture.CODEC.optionalFieldOf("texture").forGetter(Patch::body),
            ClientAsset.ResourceTexture.CODEC.optionalFieldOf("cape").forGetter(Patch::cape),
            ClientAsset.ResourceTexture.CODEC.optionalFieldOf("elytra").forGetter(Patch::elytra),
            PlayerModelType.CODEC.optionalFieldOf("model").forGetter(Patch::model)).apply(i, Patch::create));
        public static final StreamCodec<ByteBuf, Patch> STREAM_CODEC = StreamCodec.composite(
            net.minecraft.network.codec.ByteBufCodecs.optional(ClientAsset.ResourceTexture.STREAM_CODEC), Patch::body,
            net.minecraft.network.codec.ByteBufCodecs.optional(ClientAsset.ResourceTexture.STREAM_CODEC), Patch::cape,
            net.minecraft.network.codec.ByteBufCodecs.optional(ClientAsset.ResourceTexture.STREAM_CODEC), Patch::elytra,
            net.minecraft.network.codec.ByteBufCodecs.optional(PlayerModelType.STREAM_CODEC), Patch::model, Patch::create);

        public static Patch create(Optional<ClientAsset.ResourceTexture> texture, Optional<ClientAsset.ResourceTexture> capeTexture,
            Optional<ClientAsset.ResourceTexture> elytraTexture, Optional<PlayerModelType> model) {
            return texture.isEmpty() && capeTexture.isEmpty() && elytraTexture.isEmpty() && model.isEmpty() ? EMPTY
                : new Patch(texture, capeTexture, elytraTexture, model);
        }
    }
}

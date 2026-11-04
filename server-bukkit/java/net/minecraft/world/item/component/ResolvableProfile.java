// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import com.google.common.collect.LinkedHashMultimap;
import com.google.common.collect.Multimap;
import com.mojang.authlib.GameProfile;
import com.mojang.authlib.properties.Property;
import com.mojang.authlib.properties.PropertyMap;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.function.Consumer;
import net.minecraft.core.UUIDUtil;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.nbt.NbtOps;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.players.ProfileResolver;
import net.minecraft.world.entity.player.PlayerSkin;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;

public abstract class ResolvableProfile implements TooltipProvider {
    private static final Codec<Property> kPropertyCodec = RecordCodecBuilder.create(i -> i.group(
        Codec.STRING.fieldOf("name").forGetter(Property::name), Codec.STRING.fieldOf("value").forGetter(Property::value),
        Codec.STRING.optionalFieldOf("signature").forGetter(property -> Optional.ofNullable(property.signature())))
        .apply(i, (name, value, signature) -> new Property(name, value, signature.orElse(null))));
    private static final Codec<PropertyMap> kPropertiesCodec = kPropertyCodec.listOf().xmap(list -> {
        Multimap<String, Property> map = LinkedHashMultimap.create();
        list.forEach(property -> map.put(property.name(), property));
        return new PropertyMap(map);
    }, map -> List.copyOf(map.values()));
    private static final com.mojang.serialization.MapCodec<Partial> kPartialCodec = RecordCodecBuilder.mapCodec(i -> i.group(
        Codec.STRING.optionalFieldOf("name").forGetter(Partial::name), UUIDUtil.CODEC.optionalFieldOf("id").forGetter(Partial::id),
        kPropertiesCodec.optionalFieldOf("properties", PropertyMap.EMPTY).forGetter(Partial::properties)).apply(i, Partial::new));
    private static final Codec<ResolvableProfile> kObjectCodec = RecordCodecBuilder.create(i -> i.group(
        kPartialCodec.forGetter(profile -> profile.partialOf()), PlayerSkin.Patch.MAP_CODEC.forGetter(ResolvableProfile::skinPatch))
        .apply(i, ResolvableProfile::fromParts));
    public static final Codec<ResolvableProfile> CODEC = Codec.withAlternative(kObjectCodec, Codec.STRING, ResolvableProfile::createUnresolved);
    public static final StreamCodec<ByteBuf, ResolvableProfile> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.either(ByteBufCodecs.GAME_PROFILE, Partial.STREAM_CODEC), ResolvableProfile::unpack,
        PlayerSkin.Patch.STREAM_CODEC, ResolvableProfile::skinPatch, ResolvableProfile::create);

    private static ResolvableProfile create(Either<GameProfile, Partial> value, PlayerSkin.Patch patch) {
        return value.map(profile -> new Static(Either.left(profile), patch), partial -> fromParts(partial, patch));
    }

    protected final GameProfile partialProfile;
    protected final PlayerSkin.Patch skinPatch;

    public static ResolvableProfile createResolved(GameProfile gameProfile) {
        return new Static(Either.left(gameProfile), PlayerSkin.Patch.EMPTY);
    }

    public static ResolvableProfile createUnresolved(String name) {
        return new Dynamic(Either.left(name), PlayerSkin.Patch.EMPTY);
    }

    public static ResolvableProfile createUnresolved(UUID id) {
        return new Dynamic(Either.right(id), PlayerSkin.Patch.EMPTY);
    }

    private static ResolvableProfile fromParts(Partial partial, PlayerSkin.Patch patch) {
        boolean bare = partial.properties.isEmpty();
        if (bare && partial.id.isEmpty() && partial.name.isPresent()) {
            return new Dynamic(Either.left(partial.name.get()), patch);
        }
        if (bare && partial.name.isEmpty() && partial.id.isPresent()) {
            return new Dynamic(Either.right(partial.id.get()), patch);
        }
        if (partial.id.isPresent() && partial.name.isPresent()) {
            return new Static(Either.left(new GameProfile(partial.id.get(), partial.name.get(), partial.properties)), patch);
        }
        return new Static(Either.right(partial), patch);
    }

    private Partial partialOf() {
        Either<GameProfile, Partial> contents = unpack();
        return contents.map(profile -> new Partial(Optional.of(profile.name()), Optional.of(profile.id()), profile.properties()), partial -> partial);
    }

    protected abstract Either<GameProfile, Partial> unpack();

    protected ResolvableProfile(GameProfile partialProfile, PlayerSkin.Patch skinPatch) {
        this.partialProfile = partialProfile;
        this.skinPatch = skinPatch;
    }

    public abstract CompletableFuture<GameProfile> resolveProfile(ProfileResolver profileResolver);

    public GameProfile partialProfile() {
        return partialProfile;
    }

    public PlayerSkin.Patch skinPatch() {
        return skinPatch;
    }

    public abstract Optional<String> name();

    private static GameProfile placeholder(Optional<String> name, Optional<UUID> id, PropertyMap properties) {
        String text = name.orElse("");
        UUID uuid = id.orElseGet(() -> UUID.nameUUIDFromBytes(("OfflinePlayer:" + text).getBytes(StandardCharsets.UTF_8)));
        return new GameProfile(uuid, text, properties);
    }

    private static Optional<GameProfile> lookup(ProfileResolver resolver, Either<String, UUID> nameOrId) {
        return nameOrId.map(resolver::fetchByName, resolver::fetchById);
    }

    protected static record Partial(Optional<String> name, Optional<UUID> id, PropertyMap properties) {
        public static final Partial EMPTY = new Partial(Optional.empty(), Optional.empty(), PropertyMap.EMPTY);
        public static final StreamCodec<ByteBuf, Partial> STREAM_CODEC = StreamCodec.composite(
            ByteBufCodecs.optional(ByteBufCodecs.PLAYER_NAME), Partial::name, ByteBufCodecs.optional(UUIDUtil.STREAM_CODEC), Partial::id,
            ByteBufCodecs.GAME_PROFILE_PROPERTIES, Partial::properties, Partial::new);
    }

    public static final class Static extends ResolvableProfile {
        public static final Static EMPTY = new Static(Either.right(Partial.EMPTY), PlayerSkin.Patch.EMPTY);
        private final Either<GameProfile, Partial> contents;

        private Static(Either<GameProfile, Partial> contents, PlayerSkin.Patch skinPatch) {
            super(contents.map(profile -> profile, partial -> placeholder(partial.name, partial.id, partial.properties)), skinPatch);
            this.contents = contents;
        }

        @Override
        public CompletableFuture<GameProfile> resolveProfile(ProfileResolver profileResolver) {
            if (contents.left().isPresent()) {
                return CompletableFuture.completedFuture(contents.left().get());
            }
            Partial partial = contents.right().get();
            return CompletableFuture.supplyAsync(() -> {
                Optional<GameProfile> found = partial.id.isPresent() ? lookup(profileResolver, Either.right(partial.id.get()))
                    : partial.name.flatMap(name -> lookup(profileResolver, Either.left(name)));
                return found.orElseGet(() -> placeholder(partial.name, partial.id, partial.properties));
            });
        }

        @Override
        protected Either<GameProfile, Partial> unpack() {
            return contents;
        }

        @Override
        public Optional<String> name() {
            return contents.map(profile -> Optional.of(profile.name()), partial -> partial.name);
        }

        @Override
        public boolean equals(Object o) {
            return o instanceof Static other && contents.equals(other.contents) && skinPatch.equals(other.skinPatch);
        }

        @Override
        public int hashCode() {
            return Objects.hash(contents, skinPatch);
        }

        @Override
        public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {}
    }

    public static final class Dynamic extends ResolvableProfile {
        private final Either<String, UUID> nameOrId;

        private Dynamic(Either<String, UUID> nameOrId, PlayerSkin.Patch skinPatch) {
            super(placeholder(nameOrId.left(), nameOrId.right(), PropertyMap.EMPTY), skinPatch);
            this.nameOrId = nameOrId;
        }

        @Override
        public Optional<String> name() {
            return nameOrId.left();
        }

        @Override
        public boolean equals(Object o) {
            return o instanceof Dynamic other && nameOrId.equals(other.nameOrId) && skinPatch.equals(other.skinPatch);
        }

        @Override
        public int hashCode() {
            return Objects.hash(nameOrId, skinPatch);
        }

        @Override
        protected Either<GameProfile, Partial> unpack() {
            return Either.right(new Partial(nameOrId.left(), nameOrId.right(), PropertyMap.EMPTY));
        }

        @Override
        public CompletableFuture<GameProfile> resolveProfile(ProfileResolver profileResolver) {
            return CompletableFuture.supplyAsync(() -> lookup(profileResolver, nameOrId)
                .orElseGet(() -> placeholder(nameOrId.left(), nameOrId.right(), PropertyMap.EMPTY)));
        }

        @Override
        public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {}
    }
}

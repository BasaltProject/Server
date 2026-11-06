// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import com.mojang.datafixers.util.Either;
import io.netty.buffer.ByteBuf;
import java.net.URI;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public record ServerLinks(List<Entry> entries) {
    public static final ServerLinks EMPTY = new ServerLinks(List.of());
    public static final StreamCodec<ByteBuf, Either<KnownLinkType, Component>> TYPE_STREAM_CODEC = ByteBufCodecs.either(KnownLinkType.STREAM_CODEC, net.minecraft.network.chat.ComponentSerialization.TRUSTED_CONTEXT_FREE_STREAM_CODEC);
    public static final StreamCodec<ByteBuf, List<UntrustedEntry>> UNTRUSTED_LINKS_STREAM_CODEC = UntrustedEntry.STREAM_CODEC.apply(ByteBufCodecs.list());

    public boolean isEmpty() {
        return entries.isEmpty();
    }

    public Optional<Entry> findKnownType(KnownLinkType type) {
        return entries.stream().filter(entry -> entry.type.map(known -> known == type, custom -> false)).findFirst();
    }

    public List<UntrustedEntry> untrust() {
        return entries.stream().map(entry -> new UntrustedEntry(entry.type, entry.link.toString())).toList();
    }

    public static record UntrustedEntry(Either<KnownLinkType, Component> type, String link) {
        public static final StreamCodec<ByteBuf, UntrustedEntry> STREAM_CODEC = StreamCodec.composite(TYPE_STREAM_CODEC, UntrustedEntry::type, ByteBufCodecs.STRING_UTF8, UntrustedEntry::link, UntrustedEntry::new);
    }

    public static record Entry(Either<KnownLinkType, Component> type, URI link) {
        public static Entry knownType(KnownLinkType type, URI link) {
            return new Entry(Either.left(type), link);
        }

        public static Entry custom(Component displayName, URI link) {
            return new Entry(Either.right(displayName), link);
        }

        public Component displayName() {
            return type.map(KnownLinkType::displayName, name -> name);
        }
    }

    public static enum KnownLinkType {
        BUG_REPORT("report_bug"),
        COMMUNITY_GUIDELINES("community_guidelines"),
        SUPPORT("support"),
        STATUS("status"),
        FEEDBACK("feedback"),
        COMMUNITY("community"),
        WEBSITE("website"),
        FORUMS("forums"),
        NEWS("news"),
        ANNOUNCEMENTS("announcements");

        public static final StreamCodec<ByteBuf, KnownLinkType> STREAM_CODEC = ByteBufCodecs.idMapper(index -> values()[index], KnownLinkType::ordinal);
        private final Component displayName;

        KnownLinkType(String key) {
            this.displayName = Component.translatable("known_server_link." + key);
        }

        Component displayName() {
            return displayName;
        }

        public Entry create(URI link) {
            return Entry.knownType(this, link);
        }
    }
}

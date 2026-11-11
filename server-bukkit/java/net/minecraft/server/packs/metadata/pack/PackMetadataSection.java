// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.metadata.pack;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.server.packs.PackType;
import net.minecraft.util.InclusiveRange;
import net.minecraft.network.chat.Component;
import net.minecraft.server.packs.PackType;
import net.minecraft.server.packs.metadata.MetadataSectionType;
import net.minecraft.util.InclusiveRange;
public record PackMetadataSection(Component description, InclusiveRange<PackFormat> supportedFormats) {
    public static final MetadataSectionType<PackMetadataSection> CLIENT_TYPE = new MetadataSectionType<>("pack", codecFor(PackType.CLIENT_RESOURCES));
    public static final MetadataSectionType<PackMetadataSection> SERVER_TYPE = new MetadataSectionType<>("pack", codecFor(PackType.SERVER_DATA));
    public static final MetadataSectionType<PackMetadataSection> FALLBACK_TYPE = new MetadataSectionType<>("pack", RecordCodecBuilder.<PackMetadataSection>create(i -> i.group(
        ComponentSerialization.CODEC.fieldOf("description").forGetter(PackMetadataSection::description))
        .apply(i, description -> new PackMetadataSection(description, new InclusiveRange<>(PackFormat.of(Integer.MAX_VALUE))))));

    private static Codec<PackMetadataSection> codecFor(PackType type) {
        return RecordCodecBuilder.create(i -> i.group(ComponentSerialization.CODEC.fieldOf("description").forGetter(PackMetadataSection::description),
            PackFormat.packCodec(type).forGetter(PackMetadataSection::supportedFormats)).apply(i, PackMetadataSection::new));
    }
    public static MetadataSectionType<PackMetadataSection> forPackType(PackType packType) {
        return packType == PackType.CLIENT_RESOURCES ? CLIENT_TYPE : SERVER_TYPE;
    }
}

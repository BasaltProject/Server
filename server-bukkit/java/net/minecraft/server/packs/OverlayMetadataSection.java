// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.server.packs.metadata.MetadataSectionType;
import net.minecraft.server.packs.metadata.pack.PackFormat;
import net.minecraft.util.InclusiveRange;
public record OverlayMetadataSection(List<OverlayEntry> overlays) {
    public static final MetadataSectionType<OverlayMetadataSection> CLIENT_TYPE = new MetadataSectionType<>("overlays", codecForPackType(PackType.CLIENT_RESOURCES));
    public static final MetadataSectionType<OverlayMetadataSection> SERVER_TYPE = new MetadataSectionType<>("overlays", codecForPackType(PackType.SERVER_DATA));
    public static Codec<OverlayMetadataSection> codecForPackType(PackType packType) {
        return RecordCodecBuilder.create(i -> i.group(OverlayEntry.listCodecFor(packType).fieldOf("entries").forGetter(OverlayMetadataSection::overlays))
            .apply(i, OverlayMetadataSection::new));
    }
    public static MetadataSectionType<OverlayMetadataSection> forPackType(PackType packType) {
        return packType == PackType.CLIENT_RESOURCES ? CLIENT_TYPE : SERVER_TYPE;
    }
    public List<String> overlaysForVersion(PackFormat version) {
        return overlays.stream().filter(entry -> entry.isApplicable(version)).map(OverlayEntry::overlay).toList();
    }
    public static record OverlayEntry(InclusiveRange<PackFormat> format, String overlay) {
        private static Codec<List<OverlayEntry>> listCodecFor(PackType packType) {
            int lastPre = PackFormat.lastPreMinorVersion(packType);
            return IntermediateEntry.CODEC.listOf().flatXmap(
                list -> PackFormat.validateHolderList(list, lastPre, (entry, formats) -> new OverlayEntry(formats, entry.overlay())),
                list -> DataResult.success(list.stream().map(entry -> new IntermediateEntry(PackFormat.IntermediaryFormat.fromRange(entry.format(), lastPre), entry.overlay())).toList()));
        }

        public boolean isApplicable(PackFormat formatToTest) {
            return format.isValueInRange(formatToTest);
        }
        static record IntermediateEntry(PackFormat.IntermediaryFormat format, String overlay) implements PackFormat.IntermediaryFormatHolder {
            static final Codec<IntermediateEntry> CODEC = RecordCodecBuilder.create(i -> i.group(
                PackFormat.IntermediaryFormat.OVERLAY_CODEC.forGetter(IntermediateEntry::format),
                Codec.STRING.validate(name -> name.matches("[-_a-zA-Z0-9.]+") ? DataResult.success(name) : DataResult.error(() -> name + " is not accepted directory name"))
                    .fieldOf("directory").forGetter(IntermediateEntry::overlay)).apply(i, IntermediateEntry::new));

            @Override
            public String toString() {
                return overlay;
            }
        }
    }
}
